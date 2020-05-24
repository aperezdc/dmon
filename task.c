/*
 * task.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "task.h"
#include "wheel/wheel.h"
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <grp.h> /* setgroups() */


static void
_task_free (void *taskptr)
{
    task_t *task = taskptr;

    if (task->argc) {
        int i;
        assert (task->argv);

        for (i = 0; i < task->argc; i++)
            free (task->argv[i]);

        task->argv = 0;
        task->argv = NULL;
    }
}


task_t*
task_new (int argc, const char **argv)
{
    task_t template = TASK;
    task_t *task = w_obj_new (task_t);
    memcpy (((w_obj_t*) task) + 1,
            ((w_obj_t*) &template) + 1,
            sizeof (task_t) - sizeof (w_obj_t));

    if (argc) {
        int i;
        assert (argv);

        task->argv = w_alloc (char*, argc);
        for (i = 0; i < argc; i++)
            task->argv[i] = w_str_dup (argv[i]);
        task->argc = argc;
    }
    else {
        assert (argv == NULL);
    }
    return (task_t*) w_obj_dtor (task, _task_free);
}


void
task_start (task_t *task)
{
    assert (task != NULL);

    time_t now = time (NULL);
    unsigned sleep_time = (difftime (now, task->started) > 1) ? 0 : 1;

    W_DEBUG ("Last start $Is ago, will wait for $Is\n",
             (unsigned) difftime (now, task->started), sleep_time);
    memcpy (&task->started, &now, sizeof (time_t));

    if ((task->pid = fork ()) < 0)
        die ("fork failed: %s\n", ERRSTR);

    task->action = A_NONE;

    /* We got a valid PID, return */
    if (task->pid > 0) {
        W_DEBUGC ("  child pid = $I\n", (unsigned) task->pid);
        return;
    }

    /*
     * Sleep before exec'ing the child: this is needed to avoid performing
     * the classical "continued fork-exec without child reaping" DoS attack.
     * We do this here, as soon as the child has been forked.
     */
    safe_sleep (sleep_time);

    /* Execute child */
    if (task->write_fd >= 0) {
        W_DEBUGC ("  redirecting write_fd = $i -> $i\n", task->write_fd, STDOUT_FILENO);
        if (dup2 (task->write_fd, STDOUT_FILENO) < 0) {
            fprintf (stderr, "dup2() redirection failed: %s\n", ERRSTR);
            _exit (111);
        }
    }
    if (task->read_fd >= 0) {
        W_DEBUGC ("  redirecting read_fd = $i -> $i\n", task->read_fd, STDIN_FILENO);
        if (dup2 (task->read_fd, STDIN_FILENO) < 0) {
            fprintf (stderr, "dup2() redirection failed: %s\n", ERRSTR);
            _exit (111);
        }
    }

    if (task->redir_errfd) {
        W_DEBUG ("  redirecting stderr -> stdout\n");
        if (dup2 (STDOUT_FILENO, STDERR_FILENO) < 0) {
            fprintf (stderr, "dup2() failed for stderr: %s\n", ERRSTR);
            _exit (111);
        }
    }

    /* Groups must be changed first, while we have privileges */
    if (task->user.gid > 0) {
        W_DEBUG ("  set group id $I\n", (unsigned) task->pid);
        if (setgid (task->user.gid))
            die ("could not set group id: %s\n", ERRSTR);
    }

    if (task->user.ngid > 0) {
        W_DEBUG ("  calling setgroups ($I groups)\n", task->user.ngid);
        if (setgroups (task->user.ngid, task->user.gids))
            die ("could not set additional groups: %s\n", ERRSTR);
    }

    /* Now drop privileges */
    if (task->user.uid > 0) {
        W_DEBUG ("  set user id $I\n", (unsigned) task->user.uid);
        if (setuid (task->user.uid))
            die ("could not set user id: %s\n", ERRSTR);
    }

    execvp (task->argv[0], task->argv);
    _exit (111);
}


void
task_signal_dispatch (task_t *task)
{
    assert (task != NULL);

    if (task->signal == NO_SIGNAL) /* Invalid signal, nothing to do */
        return;

    W_DEBUG ("signal $i to process $I\n",
             task->signal, (unsigned) task->pid);

    if (kill (task->pid, task->signal) < 0) {
        die ("cannot send signal %i to process %lu: %s\n",
             task->signal, (unsigned long) task->pid, ERRSTR);
    }
    task_signal_queue (task, NO_SIGNAL);
}


void
task_signal (task_t *task, int signum)
{
    assert (task != NULL);

    /* Dispatch pending signal first if needed */
    task_signal_dispatch (task);

    /* Then send our own */
    task_signal_queue (task, signum);
    task_signal_dispatch (task);
}


void
task_action_dispatch (task_t *task)
{
    assert (task != NULL);

    switch (task->action) {
        case A_NONE: /* Nothing to do */
            return;
        case A_START:
            task_start (task);
            break;
        case A_STOP:
            task_action_queue (task, A_NONE);
            if (task->pid != NO_PID) {
                task_signal (task, SIGTERM);
                task_signal (task, SIGCONT);
            }
            break;
        case A_SIGNAL:
            task_action_queue (task, A_NONE);
            task_signal_dispatch (task);
            break;
    }
}


void
task_action (task_t *task, action_t action)
{
    assert (task != NULL);

    /* Dispatch pending action. */
    task_action_dispatch (task);

    /* Send our own action. */
    task_action_queue (task, action);
    task_action_dispatch (task);
}


/* vim: expandtab tabstop=4 shiftwidth=4
 */
