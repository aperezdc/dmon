/*
 * dmon.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define _BSD_SOURCE /* getloadavg() */

#include "task.h"
#include "util.h"
#include "iolib.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>


#ifndef MULTICALL
# define dmon_main main
#endif /* !MULTICALL */


static int           log_fds[2]   = { -1, -1 };
static task_t        cmd_task     = TASK;
static task_t        log_task     = TASK;
static float         load_low     = 0.0f;
static float         load_high    = 0.0f;
static int           success_exit = 0;
static int           log_signals  = 0;
static int           cmd_signals  = 0;
static unsigned long cmd_timeout  = 0;
static int           check_child  = 0;
static int           running      = 1;
static int           paused       = 0;


static const struct {
    const char *name;
    int         code;
} forward_signals[] = {
    { "CONT", SIGCONT },
    { "ALRM", SIGALRM },
    { "QUIT", SIGQUIT },
    { "USR1", SIGUSR1 },
    { "USR2", SIGUSR2 },
    { "HUP" , SIGHUP  },
    { "NONE", NO_SIGNAL },
    { "STOP", SIGSTOP },
    { "TERM", SIGTERM },
    { "INT" , SIGINT  },
    { "KILL", SIGKILL },
    { NULL  , NO_SIGNAL },
};


#define almost_zerof(_v)  ((_v) < 0.000000001f)

#define log_enabled  (log_fds[0] != -1)
#define load_enabled (!almost_zerof (load_high))


#ifdef DEBUG_TRACE
const char*
signal_to_name (int signum)
{
    static const char *unknown = "(unknown)";
    int i = 0;

    while (forward_signals[i].name != NULL) {
        if (forward_signals[i].code == signum)
            return forward_signals[i].name;
        i++;
    }
    return unknown;
}
#endif /* DEBUG_TRACE */




static void
reap_and_check (void)
{
    int status;
    pid_t pid;

    dprint (("waiting for a children to reap...\n"));

    pid = waitpid (-1, &status, WNOHANG);

    if (pid == cmd_task.pid) {
        dprint (("reaped cmd process @L\n", (unsigned) pid));

        cmd_task.pid = NO_PID;

        /*
         * If exit-on-success was request AND the process exited ok,
         * then we do not want to respawn, but to gracefully shutdown.
         */
        if (success_exit && WIFEXITED (status) && WEXITSTATUS (status) == 0) {
            dprint (("cmd process ended successfully, will exit\n"));
            running = 0;
        }
        else {
            cmd_task.action = A_START;
        }
    }
    else if (log_enabled && pid == log_task.pid) {
        dprint (("reaped log process @L\n", (unsigned) pid));
        log_task.action = A_START;
        log_task.pid = NO_PID;
    }
    else {
        dprint (("reaped unknown process @L", (unsigned) pid));
    }
}


static void
handle_signal (int signum)
{
    unsigned i = 0;

    dprint (("handle signal @c\n", signal_to_name (signum)));

    /* Receiving INT/TERM signal will stop gracefully */
    if (signum == SIGINT || signum == SIGTERM) {
        running = 0;
        return;
    }

    /* Handle CHLD: check children */
    if (signum == SIGCHLD) {
        check_child = 1;
        return;
    }

    /*
     * If we have a maximum time to run the process, and we receive SIGALRM,
     * then the timeout was reached. As per signal(7) it is safe to kill(2)
     * the process from a signal handler, so we do that and then mark it for
     * respawning.
     */
    if (cmd_timeout && signum == SIGALRM) {
        task_action (&cmd_task, A_STOP);
        cmd_task.action = A_START;
        alarm (cmd_timeout);
        return;
    }

    while (forward_signals[i].code != NO_SIGNAL) {
        if (signum == forward_signals[i++].code)
            break;
    }

    if (signum != NO_SIGNAL) {
        /* Try to forward signals */
        if (cmd_signals) {
            dprint (("delayed signal @i for cmd process\n", signum));
            cmd_task.action = A_SIGNAL;
            cmd_task.signal = signum;
        }
        if (log_signals && log_enabled) {
            dprint (("delayed signal @i for log process\n", signum));
            log_task.action = A_SIGNAL;
            log_task.signal = signum;
        }
    }

    return;
}



static void
setup_signals (void)
{
    unsigned i = 0;
    struct sigaction sa;

    sa.sa_handler = handle_signal;
    sa.sa_flags = SA_NOCLDSTOP;
    sigfillset(&sa.sa_mask);

    while (forward_signals[i].code!= NO_SIGNAL) {
        safe_sigaction (forward_signals[i].name,
                        forward_signals[i].code, &sa);
        i++;
    }

    safe_sigaction ("CHLD", SIGCHLD, &sa);
    safe_sigaction ("TERM", SIGTERM, &sa);
    safe_sigaction ("INT" , SIGINT , &sa);
}



#define _dmon_help_message                                           \
    "Usage: @c [options] cmd [cmd-options] [-- log [log-options]]\n" \
    "Launch a simple daemon, providing logging and respawning.\n"    \
    "\n"                                                             \
    "Process monitorization:\n"                                      \
    "\n"                                                             \
    "  -p PATH    Write PID to the a file in the given PATH.\n"      \
    "  -n         Do not daemonize, stay in foreground.\n"           \
    "  -s         Forward signals to command process.\n"             \
    "  -S         Forward signals to log process.\n"                 \
    "  -1         Exit if command exits with a zero return code.\n"  \
    "\n"                                                             \
    "Process execution environment:\n"                               \
    "\n"                                                             \
    "  -u UID[:GID...]  User and groups to run process as.\n"        \
    "  -U UID[:GID...]  User and groups to run the log process as.\n"\
    "  -e               Redirect command stderr to stdout.\n"        \
    "\n"                                                             \
    "Process execution constraints:\n"                               \
    "\n"                                                             \
    "  -t TIME    If command takes longer than TIME, restart it.\n"  \
    "  -L VALUE   Stop process when system load reaches VALUE.\n"    \
    "  -l VALUE   Resume process execution when system load drops\n" \
    "             below VALUE. If not given defaults to half the\n"  \
    "             value of the value specified with '-L'.\n"         \
    "\n"                                                             \
    "Getting help:\n"                                                \
    "\n"                                                             \
    "  -h, -?     Show this help text.\n"                            \
    "\n"


int
dmon_main (int argc, char **argv)
{
	const char *pidfile = NULL;
	int pidfile_fd = -1;
	int daemonize = 1;
	char c;
	int i;

	while ((c = getopt (argc, argv, "+?heSsnp:1t:u:U:l:L:")) != -1) {
		switch (c) {
            case 'p': pidfile = optarg; break;
            case '1': success_exit = 1; break;
            case 'e': cmd_task.redir_errfd = 1; break;
            case 's': cmd_signals = 1; break;
            case 'S': log_signals = 1; break;
            case 'n': daemonize = 0; break;
            case 't':
                if (parse_time_arg (optarg, &cmd_timeout))
                    die ("@c: Invalid time value '@c'", argv[0], optarg);
                break;
            case 'u':
                if (parse_uidgids (optarg, &cmd_task.user))
                    die ("@c: Invalid user/groups '@c'", argv[0], optarg);
                break;
            case 'U':
                if (parse_uidgids (optarg, &log_task.user))
                    die ("@c: Invalid user/groups '@c'", argv[0], optarg);
                break;
            case 'l':
                if (parse_float_arg (optarg, &load_low))
                    die ("@c: Invalid number '@c'", argv[0], optarg);
                break;
            case 'L':
                if (parse_float_arg (optarg, &load_high))
                    die ("@c: Invalid number '@c'", argv[0], optarg);
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

    if (load_enabled && almost_zerof (load_low))
        load_low = load_high / 2.0f;

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
        if (pipe (log_fds) != 0) {
            die ("@c: Cannot create pipe: @E", argv[0]);
        }
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

    if (cmd_task.argc == 0)
        die ("@c: No command to run given.", argv[0]);

    if (pidfile) {
        pidfile_fd = open (pidfile, O_TRUNC | O_CREAT | O_WRONLY, 0666);
        if (pidfile_fd < 0) {
            die ("@c: cannot open '@c' for writing: @E", argv[0], pidfile);
        }
    }

    if (daemonize)
        become_daemon ();

    /* We have a valid file descriptor: write PID */
    if (pidfile_fd >= 0) {
        format (pidfile_fd, "@L\n", (unsigned) getpid ());
        close (pidfile_fd);
    }

    setup_signals ();
    alarm (cmd_timeout);

    cmd_task.write_fd = log_fds[1];
    log_task.read_fd  = log_fds[0];

    while (running) {
        dprint ((">>> loop iteration\n"));
        if (check_child) {
            reap_and_check ();

            /* reap_and_check() may request stopping on successful exit */
            if (!running) break;
        }

        task_action_dispatch (&cmd_task);
        if (log_enabled)
            task_action_dispatch (&log_task);

        if (load_enabled) {
            double load_cur;

            dprint (("checking load after sleeping 1s\n"));
            interruptible_sleep (1);

            if (getloadavg (&load_cur, 1) == -1)
                die ("@c: Could not get load average!");

            if (paused) {
                /* If the current load dropped below load_low -> resume */
                if (load_cur <= load_low) {
                    dprint (("resuming... "));
                    task_signal (&cmd_task, SIGCONT);
                    paused = 0;
                }
            }
            else {
                /* If the load went above load_high -> pause */
                if (load_cur > load_high) {
                    dprint (("pausing... "));
                    task_signal (&cmd_task, SIGSTOP);
                    paused = 1;
                }
            }
        }
        else {
            /* Wait for signals to arrive. */
            dprint (("waiting for signals to come...\n"));
            pause ();
        }
    }

    dprint (("exiting gracefully...\n"));

    task_action (&cmd_task, A_STOP);
    if (log_enabled)
        task_action (&log_task, A_STOP);

	exit (EXIT_SUCCESS);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
