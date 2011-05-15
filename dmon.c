/*
 * dmon.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define _BSD_SOURCE             /* getloadavg() */
#define _POSIX_C_SOURCE 199309L /* nanosleep()  */

#include "wheel.h"
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
#include <errno.h>
#include <time.h>


#ifndef MULTICALL
# define dmon_main main
#endif /* !MULTICALL */


static int           log_fds[2]   = { -1, -1 };
static int           status_fd    = -1;
static task_t        cmd_task     = TASK;
static task_t        log_task     = TASK;
static float         load_low     = 0.0f;
static float         load_high    = 0.0f;
static wbool         success_exit = W_NO;
static wbool         log_signals  = W_NO;
static wbool         cmd_signals  = W_NO;
static unsigned long cmd_timeout  = 0;
static unsigned long cmd_interval = 0;
static int           check_child  = 0;
static int           running      = 1;
static int           paused       = 0;
static wbool         nodaemon     = W_NO;
static char         *status_path  = NULL;
static char         *pidfile_path = NULL;


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

#define log_enabled   (log_fds[0] != -1)
#define load_enabled  (!almost_zerof (load_high))


static inline void
_write_status (const char *fmt, ...)
{
    va_list arg;

    assert (status_fd >= 0);

    va_start (arg, fmt);
    vformat (status_fd, fmt, arg);
    va_end (arg);
}

#define write_status(_args) \
    if (status_fd != -1)    \
        _write_status _args


#define task_action_dispatch_and_write_status(_what, _task)         \
    do {                                                            \
        int __pidafter__ = 0;                                       \
        if (status_fd != -1) {                                      \
            switch ((_task)->action) {                              \
                case A_NONE:                                        \
                    break;                                          \
                case A_START:                                       \
                    _write_status ("@c start ", (_what));           \
                    __pidafter__ = 1;                               \
                    break;                                          \
                case A_STOP:                                        \
                    _write_status ("@c stop @L\n", (_what),         \
                                   (unsigned long) ((_task)->pid)); \
                    break;                                          \
                case A_SIGNAL:                                      \
                    _write_status ("@c signal @L @i\n",             \
                                   (unsigned long) ((_task)->pid),  \
                                   (_task)->signal);                \
                    break;                                          \
            }                                                       \
        }                                                           \
        task_action_dispatch (_task);                               \
        if (__pidafter__ && status_fd != -1)                        \
            _write_status ("@L\n", (unsigned long) ((_task)->pid)); \
    } while (0)


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


static int
reap_and_check (void)
{
    int status;
    pid_t pid;

    dprint (("waiting for a children to reap...\n"));

    pid = waitpid (-1, &status, WNOHANG);

    if (pid == cmd_task.pid) {
        dprint (("reaped cmd process @L\n", (unsigned) pid));

        write_status (("cmd exit @L @i\n", (unsigned long) pid, status));

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
        return status;
    }
    else if (log_enabled && pid == log_task.pid) {
        dprint (("reaped log process @L\n", (unsigned) pid));

        write_status (("log exit @L @i\n", (unsigned long) pid, status));

        log_task.action = A_START;
        log_task.pid = NO_PID;
    }
    else {
        dprint (("reaped unknown process @L", (unsigned) pid));
    }

    /*
     * For cases where a return status is not meaningful (PIDs other than
     * that of the command being run) just return some invalid return code
     * value.
     */
    return -1;
}


static void
handle_signal (int signum)
{
    unsigned i = 0;

    dprint (("handle signal @i (@c)\n", signum, signal_to_name (signum)));

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
        write_status (("cmd timeout @L\n", (unsigned long) cmd_task.pid));
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


static w_opt_status_t
_environ_option (const w_opt_context_t *ctx)
{
    char *equalsign;
    char *varname;

    w_assert (ctx);
    w_assert (ctx->argument);
    w_assert (ctx->argument[0]);

    if ((equalsign = strchr (ctx->argument[0], '=')) == NULL) {
        unsetenv (ctx->argument[0]);
    }
    else {
        varname = w_str_dupl (ctx->argument[0],
                              equalsign - ctx->argument[0]);
        setenv (varname, equalsign + 1, 1);
    }
    return W_OPT_OK;
}


static w_opt_status_t
_rlimit_option (const w_opt_context_t *ctx)
{
    long value;
    int status;
    int limit;

    w_assert (ctx);
    w_assert (ctx->argument);
    w_assert (ctx->argument[0]);

    status = parse_limit_arg (ctx->argument[0], &limit, &value);
    if (status < 0)
        return W_OPT_EXIT_OK;
    if (status)
        return W_OPT_BAD_ARG;

    safe_setrlimit (limit, value);
    return W_OPT_OK;
}


static w_opt_status_t
_store_uidgids_option (const w_opt_context_t *ctx)
{
    w_assert (ctx);
    w_assert (ctx->userdata);
    w_assert (ctx->argument);
    w_assert (ctx->argument[0]);

    return (parse_uidgids (ctx->argument[0], ctx->option->extra))
            ? W_OPT_BAD_ARG
            : W_OPT_OK;
}


static w_opt_status_t
_time_option (const w_opt_context_t *ctx)
{
    w_assert (ctx);
    w_assert (ctx->argument);
    w_assert (ctx->argument[0]);
    w_assert (ctx->option);
    w_assert (ctx->option->extra);

    return (parse_time_arg (ctx->argument[0], ctx->option->extra))
           ? W_OPT_BAD_ARG
           : W_OPT_OK;
}


static w_opt_status_t
_config_option (const w_opt_context_t *ctx)
{
    w_assert (ctx);
    w_assert (ctx->argv);
    w_assert (ctx->argv[0]);

    format (fd_err,
            "@c: Option --config/-C must be the first one specified\n",
            ctx->argv[0]);

    return W_OPT_EXIT_FAIL;
}


static const w_opt_t dmon_options[] = {
    { 1, 'C' | W_OPT_CLI_ONLY, "config", _config_option, NULL,
        "Read options from the specified configuration file. If given, this option "
        "must be the first one in the command line." },

    { 1, 'I', "write-info", W_OPT_STRING, &status_path,
        "Write information on process status to the given file. "
        "Sockets and FIFOs may be used." },

    { 1, 'p', "pid-file", W_OPT_STRING, &pidfile_path,
        "Write PID to a file in the given path." },

    { 0, 'n', "no-daemon", W_OPT_BOOL, &nodaemon,
        "Do not daemonize, stay in foreground." },

    { 0, 'e', "stderr-redir", W_OPT_BOOL, &cmd_task.redir_errfd,
        "Redirect command's standard error stream to its standard "
        "output stream." },

    { 0, 's', "cmd-sigs", W_OPT_BOOL, &cmd_signals,
        "Forward signals to command process." },

    { 0, 'S', "log-sigs", W_OPT_BOOL, &log_signals,
        "Forward signals to log process." },

    { 1, 'E', "environ", _environ_option, NULL,
        "Define an environment variable, or if no value is given, "
        "delete it. This option can be specified multiple times." },

    { 1, 'r', "limit", _rlimit_option, NULL,
        "Sets a resource limit, given as 'name=value'. This option "
        "can be specified multiple times. Use '-r help' for a list." },

    { 1, 'u', "cmd-user", _store_uidgids_option, &cmd_task.user,
        "User and (optionally) groups to run the command as. Format "
        "is 'user[:group1[:group2[:...groupN]]]'." },

    { 1, 'U', "log-user", _store_uidgids_option, &log_task.user,
        "User and (optionally) groups to run the log process as. "
        "Format is 'user[:group1[:group2[:...groupN]]]'." },

    { 0, '1', "once", W_OPT_BOOL, &success_exit,
        "Exit if command exits with a zero return code. The process "
        "will be still respawned when it exits with a non-zero code." },

    { 1, 't', "timeout", _time_option, &cmd_timeout,
        "If command execution takes longer than the time specified "
        "the process will be killed and started again." },

    { 1, 'i', "interval", _time_option, &cmd_interval,
        "Time to wait between successful command executions. When "
        "exit code is non-zero, the interval is ignored and the "
        "command is executed again as soon as possible." },

    { 1, 'L', "load-high", W_OPT_FLOAT, &load_high,
        "Stop process when system load surpasses the given value." },

    { 1, 'l', "load-low", W_OPT_FLOAT, &load_low,
        "Resume process execution when system load drops below the "
        "given value. If not given, defaults to half the value passed "
        "to '-l'." },

    W_OPT_END
};


int
dmon_main (int argc, char **argv)
{
	const char *pidfile = NULL;
	char *opts_env = NULL;
	int pidfile_fd = -1;
	wbool success;
	unsigned i, consumed;

    /* Check for "-C configfile" given in the command line. */
    if (argc > 2 && ((argv[1][0] == '-' &&
                      argv[1][1] == 'C' &&
                      argv[1][2] == '\0') ||
                     !strcmp ("--config", argv[1])))

    {
        FILE *cfg_file = fopen (argv[2], "rb");
        w_io_t *cfg_io;
        char *err_msg = NULL;

        if (!cfg_file)
            w_die ("$s: Could not open file '$s', $E\n", argv[0], argv[2]);

        cfg_io = w_io_stdio_open (cfg_file);
        success = w_opt_parse_io (dmon_options, cfg_io, &err_msg);
        w_obj_unref (cfg_io);

        if (!success || err_msg)
            w_die ("$s: Error parsing '$s' at line $s\n", argv[0], argv[2], err_msg);

        replace_args_shift (2, &argc, &argv);
    }

    if ((opts_env = getenv ("DMON_OPTIONS")) != NULL)
        replace_args_string (opts_env, &argc, &argv);

    i = consumed = w_opt_parse (dmon_options, NULL, NULL, argc, argv);
    dprint (("w_opt_parse consumed @I arguments\n", consumed));

    if (status_path) {
        status_fd = open (status_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (status_fd < 0)
            w_die ("$s: Cannot open '$s' for writing, $E\n", argv[0], optarg);
    }

    if (cmd_interval && success_exit)
        w_die ("$s: Options '-i' and '-1' cannot be used together.\n", argv[0]);

    if (load_enabled && almost_zerof (load_low))
        load_low = load_high / 2.0f;

	cmd_task.argv = argv + consumed;

	/* Skip over until "--" is found */
	while (i < (unsigned) argc && strcmp (argv[i], "--") != 0) {
		cmd_task.argc++;
		i++;
	}

	/* There is a log command */
	if (i < (unsigned) argc && strcmp (argv[i], "--") == 0) {
		log_task.argc = argc - cmd_task.argc - consumed - 1;
		log_task.argv = argv + argc - log_task.argc;
        log_task.argv[log_task.argc] = NULL;
	}

    cmd_task.argv[cmd_task.argc] = NULL;

    if (log_task.argc > 0) {
        if (pipe (log_fds) != 0) {
            w_die ("$s: Cannot create pipe: $E\n", argv[0]);
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
        w_die ("$s: No command to run given.\n", argv[0]);

    if (pidfile) {
        pidfile_fd = open (pidfile, O_TRUNC | O_CREAT | O_WRONLY, 0666);
        if (pidfile_fd < 0) {
            w_die ("$s: cannot open '$s' for writing: $E\n", argv[0], pidfile);
        }
    }

    if (!nodaemon)
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
            int retcode = reap_and_check ();

            /*
             * Wait the specified timeout but DO NOT use safe_sleep(): here
             * we want an interruptible sleep-wait so reaction to signals is
             * quick, which we definitely want for SIGINT/SIGTERM.
             */
            if (cmd_interval && !success_exit && retcode == 0) {
                int retval;
                struct timespec ts;
                ts.tv_sec = cmd_interval;
                ts.tv_nsec = 0;

                do {
                    retval = nanosleep (&ts, &ts);
                    dprint (("nanosleep -> @i\n", retval));
                } while (retval == -1 && errno == EINTR && running);
            }

            /*
             * Either handling signals which interrupt the previous loop,
             * or reap_and_check() may request stopping on successful exit
             */
            if (!running) {
                cmd_task.action = A_NONE;
                break;
            }
        }

        task_action_dispatch_and_write_status ("cmd", &cmd_task);
        if (log_enabled)
            task_action_dispatch_and_write_status ("log", &log_task);

        if (load_enabled) {
            double load_cur;

            dprint (("checking load after sleeping 1s\n"));
            interruptible_sleep (1);

            if (getloadavg (&load_cur, 1) == -1)
                w_die ("$s: Could not get load average!\n");

            if (paused) {
                /* If the current load dropped below load_low -> resume */
                if (load_cur <= load_low) {
                    dprint (("resuming... "));
                    task_signal (&cmd_task, SIGCONT);
                    write_status (("cmd resume @L\n", (unsigned long) cmd_task.pid));
                    paused = 0;
                }
            }
            else {
                /* If the load went above load_high -> pause */
                if (load_cur > load_high) {
                    dprint (("pausing... "));
                    task_signal (&cmd_task, SIGSTOP);
                    write_status (("cmd pause @L\n", (unsigned long) cmd_task.pid));
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

    if (cmd_task.pid != NO_PID) {
        write_status (("cmd stop @L\n", (unsigned long) cmd_task.pid));
        task_action (&cmd_task, A_STOP);
    }
    if (log_enabled && log_task.pid != NO_PID) {
        write_status (("log stop @L\n", (unsigned long) log_task.pid));
        task_action (&log_task, A_STOP);
    }

    if (status_fd != -1)
        close (status_fd);

	exit (EXIT_SUCCESS);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
