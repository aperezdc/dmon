/*
 * task.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "task.h"
#include "wheel.h"
#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <grp.h> /* setgroups() */


void
task_start (task_t *task)
{
    time_t now = time (NULL);
    unsigned sleep_time = (difftime (now, task->started) > 1) ? 0 : 1;

    assert (task != NULL);

    w_debug (("Last start $Is ago, will wait for $Is\n",
             (unsigned) difftime (now, task->started), sleep_time));
    memcpy (&task->started, &now, sizeof (time_t));

    if ((task->pid = fork ()) < 0)
        w_die ("fork failed: $E\n");

    task->action = A_NONE;

    /* We got a valid PID, return */
    if (task->pid > 0) {
        w_debug (("child pid = $I\n", (unsigned) task->pid));
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
        w_debug (("redirecting write_fd = $i -> $i\n", task->write_fd, STDOUT_FILENO));
        if (dup2 (task->write_fd, STDOUT_FILENO) < 0) {
            w_debug (("redirection failed: $E\n"));
            _exit (111);
        }
    }
    if (task->read_fd >= 0) {
        w_debug (("redirecting read_fd = $i -> $i\n", task->read_fd, STDIN_FILENO));
        if (dup2 (task->read_fd, STDIN_FILENO) < 0) {
            w_debug (("redirection failed: $E\n"));
            _exit (111);
        }
    }

    if (task->redir_errfd) {
        w_debug (("redirecting stderr -> stdout\n"));
        if (dup2 (STDOUT_FILENO, STDERR_FILENO) < 0) {
            w_debug (("could not redirect stderr: $E\n"));
            exit (111);
        }
    }

    /* Groups must be changed first, whilw we have privileges */
    if (task->user.gid > 0) {
        w_debug (("set group id $I\n", (unsigned) task->pid));
        if (setgid (task->user.gid))
            w_die ("could not set groud id: $E\n");
    }

    if (task->user.ngid > 0) {
        w_debug (("calling setgroups ($I groups)\n", task->user.ngid));
        if (setgroups (task->user.ngid, task->user.gids))
            w_die ("could not set additional groups: $E\n");
    }

    /* Now drop privileges */
    if (task->user.uid > 0) {
        w_debug (("set user id $I\n", (unsigned) task->user.uid));
        if (setuid (task->user.uid))
            w_die ("could not set user id: $E\n");
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

    w_debug (("dispatch signal $i to process $I\n",
             task->signal, (unsigned) task->pid));

    if (kill (task->pid, task->signal) < 0) {
        w_die ("cannot send signal $i to process $L: $E\n",
             task->signal, (unsigned long) task->pid);
    }
    task->signal = NO_SIGNAL;
}


void
task_signal (task_t *task, int signum)
{
    assert (task != NULL);

    /* Dispatch pending signal first if needed */
    task_signal_dispatch (task);

    /* Then send our own */
    task->signal = signum;
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
            task->action = A_NONE;
            if (task->pid != NO_PID) {
                task_signal (task, SIGTERM);
                task_signal (task, SIGCONT);
            }
            break;
        case A_SIGNAL:
            task->action = A_NONE;
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
    task->action = action;
    task_action_dispatch (task);
}


/* vim: expandtab tabstop=4 shiftwidth=4
 */
