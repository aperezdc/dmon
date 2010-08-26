/*
 * dmon.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define _BSD_SOURCE /* getloadavg() */

#include "util.h"
#include "iolib.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>


typedef enum {
    A_NONE = 0,
    A_START,
    A_STOP,
    A_SIGNAL,
} action_t;


typedef struct {
    pid_t    pid;
    action_t action;
    int      argc;
    char   **argv;
    int      write_fd;
    int      read_fd;
    int      signal;
    time_t   started;
    uid_t    uid;
    gid_t    gid;
} task_t;

#define NO_PID    (-1)
#define NO_SIGNAL (-1)
#define TASK      { NO_PID, A_START, 0, NULL, -1, -1, NO_SIGNAL, 0, 0, 0 }

static int           log_fds[2]   = { -1, -1 };
static task_t        cmd_task     = TASK;
static task_t        log_task     = TASK;
static float         load_low     = 0.0f;
static float         load_high    = 0.0f;
static int           success_exit = 0;
static int           redir_errfd  = 0;
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
    { "TERM", SIGTERM },
    { "INT" , SIGINT  },
    { "KILL", SIGKILL },
    { "CONT", SIGCONT },
    { "STOP", SIGSTOP },
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
task_start (task_t *task)
{
    time_t now = time (NULL);
    unsigned sleep_time = (difftime (now, task->started) > 1) ? 0 : 1;

    assert (task != NULL);

    dprint (("Last start @Is ago, will wait for @Is\n",
             (unsigned) difftime (now, task->started), sleep_time));
    memcpy (&task->started, &now, sizeof (time_t));

    if ((task->pid = fork ()) < 0)
        die ("fork failed: @E");

    task->action = A_NONE;

    /* We got a valid PID, return */
    if (task->pid > 0) {
        dprint (("child pid = @L\n", (unsigned) task->pid));
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

    if (task->gid > 0) {
        dprint (("set group id @L\n", task->pid));
        if (setgid (task->gid))
            die ("could not set groud id: @E");
    }

    if (task->uid > 0) {
        dprint (("set user id @L\n", task->uid));
        if (setuid (task->uid))
            die ("could not set user id: @E");
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

    dprint (("dispatch signal @i to process @L\n",
             task->signal, (unsigned) task->pid));

    if (kill (task->pid, task->signal) < 0) {
        die ("cannot send signal @i to process @L: @E",
             task->signal, (unsigned) task->pid);
    }
    task->signal = NO_SIGNAL;
}


static void
task_signal (task_t *task, int signum)
{
    assert (task != NULL);

    /* Dispatch pending signal first if needed */
    task_signal_dispatch (task);

    /* Then send our own */
    task->signal = signum;
    task_signal_dispatch (task);
}


static void
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


static void
task_action (task_t *task, action_t action)
{
    assert (task != NULL);

    /* Dispatch pending action. */
    task_action_dispatch (task);

    /* Send our own action. */
    task->action = action;
    task_action_dispatch (task);
}


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
safe_sigaction (const char *name, int signum, struct sigaction *sa)
{
    if (sigaction (signum, sa, NULL) < 0) {
        die ("could not set handler for signal @c (@i): @E",
             name, signum);
    }
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


static void
become_daemon (void)
{
    pid_t pid;
    int nullfd = open ("/dev/null", O_RDWR, 0);

    if (nullfd < 0)
        die ("cannot daemonize, unable to open '/dev/null': @E");

    fd_cloexec (nullfd);

    if (dup2 (nullfd, fd_in) < 0)
        die ("cannot daemonize, unable to redirect stdin: @E");
    if (dup2 (nullfd, fd_out) < 0)
        die ("cannot daemonize, unable to redirect stdout: @E");
    if (dup2 (nullfd, fd_err) < 0)
        die ("cannot daemonize, unable to redirect stderr: @E");

    pid = fork ();

    if (pid < 0) die ("cannot daemonize: @E");
    if (pid > 0) _exit (EXIT_SUCCESS);

    if (setsid () == -1)
        _exit (111);
}


static int
parse_time_arg (const char *str, unsigned long *result)
{
    char *endpos = NULL;

    assert (str != NULL);
    assert (result != NULL);

    *result = strtoul (str, &endpos, 0);
    if (endpos == NULL || *endpos == '\0')
        return 0;

    switch (*endpos) {
        case 'w': *result *= 60 * 60 * 24 * 7; break;
        case 'd': *result *= 60 * 60 * 24; break;
        case 'h': *result *= 60 * 60; break;
        case 'm': *result *= 60; break;
        default: return 1;
    }

    return 0;
}


static int
parse_float_arg (const char *str, float *result)
{
    assert (str != NULL);
    assert (result != NULL);
    return !(sscanf (str, "%f", result) == 1);
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
    "  -u UID     User id/name to run process as.\n"                 \
    "  -U UID     User id/name to run the log process as.\n"         \
    "  -g GID     Group id/name to run the process as.\n"            \
    "  -G GID     Group id/name to run the log process as.\n"        \
    "  -e         Redirect command stderr to stdout.\n"              \
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
main (int argc, char **argv)
{
	const char *pidfile = NULL;
	int pidfile_fd = -1;
	int daemonize = 1;
	char c;
	int i;

	while ((c = getopt (argc, argv, "+?heSsnp:1t:u:U:g:G:l:L:")) != -1) {
		switch (c) {
            case 'p': pidfile = optarg; break;
            case '1': success_exit = 1; break;
            case 'e': redir_errfd = 1; break;
            case 's': cmd_signals = 1; break;
            case 'S': log_signals = 1; break;
            case 'n': daemonize = 0; break;
            case 't':
                if (parse_time_arg (optarg, &cmd_timeout))
                    die ("@c: Invalid time value '@c'", argv[0], optarg);
                break;
            case 'u':
                if (name_to_uid (optarg, &cmd_task.uid))
                    die ("@c: Invalid user '@c'", argv[0], optarg);
                break;
            case 'U':
                if (name_to_uid (optarg, &log_task.uid))
                    die ("@c: Invalid user '@c'", argv[0], optarg);
                break;
            case 'g':
                if (name_to_gid (optarg, &cmd_task.gid))
                    die ("@c: Invalid group '@c'", argv[0], optarg);
                break;
            case 'G':
                if (name_to_gid (optarg, &log_task.gid))
                    die ("@c: Invalid group '@c'", argv[0], optarg);
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
