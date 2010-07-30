/*
 * dmon.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "util.h"
#include "iolib.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>


#define _dmon_help_message \
    "Usage: @c [options] cmd [cmd-options] -- log [log-options]\n" \
    "Launch a simple daemon, providing logging and respawning.\n" \
    "\n" \
    "  -e         Redirect command stderr to stdout.\n" \
    "  -h, -?     Show this help text.\n" \
    "\n"


typedef enum {
    A_NONE = 0,
    A_START,
    A_STOP,
} action_t;


typedef struct {
    pid_t    pid;
    action_t action;
    int      argc;
    char   **argv;
    int      write_fd;
    int      read_fd;
    int      signal;
} task_t;

#define TASK  { 0, A_START, 0, NULL, -1, -1, -1 }

#define NO_SIGNAL (-1)

static int    log_fds[2]  = { -1, -1 };
static task_t cmd_task    = TASK;
static task_t log_task    = TASK;
static int    redir_errfd = 0;
static int    running     = 1;

#define log_enabled  (log_fds[0] != -1)


static void
task_start (task_t *task)
{
    assert (task != NULL);

    if ((task->pid = fork ()) < 0)
        die ("fork failed: @E");

    task->action = A_NONE;

    /* We got a valid PID, return */
    if (task->pid > 0) {
        dprint (("child pid = @L\n", (unsigned) task->pid));
        return;
    }

    /* Execute child */
    if (task->write_fd >= 0) {
        dprint (("redirecting write_fd = @i -> @i\n", task->write_fd, fd_out));
        if (dup2 (task->write_fd, fd_out) < 0) {
            dprint (("redirection failed: @E\n"));
            _exit (111);
        }
    }
    if (task->read_fd >= 0) {
        dprint (("redirecting read_fd = @i -> @i\n", task->read_fd, fd_in));
        if (dup2 (task->read_fd, fd_in) < 0) {
            dprint (("redirection failed: @E\n"));
            _exit (111);
        }
    }

    if (redir_errfd) {
        dprint (("redirecting stderr -> stdout\n"));
        if (dup2 (fd_out, fd_err) < 0) {
            dprint (("could not redirect stderr: @E\n"));
            exit (111);
        }
    }

    execvp (task->argv[0], task->argv);
    _exit (111);
}


static void
task_signal_dispatch (task_t *task)
{
    assert (task != NULL);

    if (task->signal == NO_SIGNAL) /* Invalid signal, nothing to do */
        return;

    if (kill (task->pid, task->signal) < 0) {
        die ("cannot send signal @i to process @L: @E",
             task->signal, (unsigned) task->pid);
    }
    task->signal = NO_SIGNAL;
}


static void
task_signal (task_t *task, int signal)
{
    assert (task != NULL);

    if (signal == NO_SIGNAL)
        return;

    /* Dispatch pending signal first if needed */
    task_signal_dispatch (task);

    /* Then send our own */
    task->signal = signal;
    task_signal_dispatch (task);
}


static void
task_action (task_t *task)
{
    switch (task->action) {
        case A_NONE: /* Nothing to do */
            return;
        case A_START:
            task_start (task);
            break;
        case A_STOP:
            task->action = A_NONE;
            task_signal (task, SIGTERM);
            task_signal (task, SIGCONT);
            break;
    }
}



int
main (int argc, char **argv)
{
	char c;
	int i;

	while ((c = getopt (argc, argv, "+?he")) != -1) {
		switch (c) {
            case 'e':
                redir_errfd = 1;
                break;
			case 'h':
			case '?':
				format (fd_out, _dmon_help_message, argv[0]);
				exit (EXIT_SUCCESS);
			default:
				format (fd_err, "@c: unrecognized option '@c'\n", argv[0], optarg);
				format (fd_err, _dmon_help_message, argv[0]);
				exit (EXIT_FAILURE);
		}
	}


	cmd_task.argv = argv + optind;
    i = optind;

	/* Skip over until "--" is found */
	while (i < argc && strcmp (argv[i], "--") != 0) {
		cmd_task.argc++;
		i++;
	}

	/* There is a log command */
	if (i < argc && strcmp (argv[i], "--") == 0) {
		log_task.argc = argc - cmd_task.argc - optind - 1;
		log_task.argv = argv + argc - log_task.argc;
        log_task.argv[log_task.argc] = NULL;
	}

    cmd_task.argv[cmd_task.argc] = NULL;

    if (log_task.argc > 0) {
        pipe (log_fds);
        dprint (("pipe_read = @i, pipe_write = @i\n", log_fds[0], log_fds[1]));
        fd_cloexec (log_fds[0]);
        fd_cloexec (log_fds[1]);
    }

#ifdef DEBUG_TRACE
    {
        char **xxargv = cmd_task.argv;
        format (fd_err, "cmd:");
        while (*xxargv) format (fd_err, " @c", *xxargv++);
        format (fd_err, "\n");
        if (log_enabled) {
            char **xxargv = log_task.argv;
            format (fd_err, "log:");
            while (*xxargv) format (fd_err, " @c", *xxargv++);
            format (fd_err, "\n");
        }
    }
#endif /* DEBUG_TRACE */

    cmd_task.write_fd = log_fds[1];
    log_task.read_fd  = log_fds[0];

    while (running) {
        task_action (&cmd_task);
        if (log_enabled)
            task_action (&log_task);

        /* Wait for signals to arrive. */
        pause ();
    }

	exit (EXIT_SUCCESS);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
