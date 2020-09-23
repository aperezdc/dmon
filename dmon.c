/*
 * dmon.c
 * Copyright (C) 2010-2014 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define _GNU_SOURCE 1

#include "deps/cflag/cflag.h"
#include "deps/clog/clog.h"
#include "conf.h"
#include "task.h"
#include "util.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>


#if !(defined(MULTICALL) && MULTICALL)
# define dmon_main main
#endif /* !MULTICALL */


static int           log_fds[2]   = { -1, -1 };
static FILE         *status_file  = NULL;
static task_t        cmd_task     = TASK;
static task_t        log_task     = TASK;
static float         load_low     = 0.0f;
static float         load_high    = 0.0f;
static bool          success_exit = false;
static int           num_respawns = -1;
static bool          log_signals  = false;
static bool          cmd_signals  = false;
static unsigned long cmd_timeout  = 0;
static unsigned long cmd_interval = 0;
static int           check_child  = 0;
static int           running      = 1;
static int           paused       = 0;
static bool          nodaemon     = false;
static char         *status_path  = NULL;
static char         *pidfile_path = NULL;
static char         *workdir_path = NULL;


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


__attribute__((format (printf, 1, 2)))
static inline void
_write_status (const char *fmt, ...)
{
    assert (status_file);

    va_list arg;
    va_start (arg, fmt);
    ssize_t r = vfprintf (status_file, fmt, arg);
    va_end (arg);

    if (r < 0)
        clog_warning("Writing to status file: %s.", ERRSTR);
}

#define write_status(...) \
    if (status_file != NULL) \
        _write_status(__VA_ARGS__)


#define task_action_dispatch_and_write_status(_what, _task)      \
    do {                                                          \
        int __pidafter__ = 0;                                     \
        if (status_file != NULL) {                                \
            switch ((_task)->action) {                            \
                case A_NONE:                                      \
                    break;                                        \
                case A_START:                                     \
                    _write_status ("%s start ", (_what));         \
                    __pidafter__ = 1;                             \
                    break;                                        \
                case A_STOP:                                      \
                    _write_status ("%s stop %li\n", (_what),      \
                                   (long) ((_task)->pid));        \
                    break;                                        \
                case A_SIGNAL:                                    \
                    _write_status ("%s signal %li %i\n", (_what), \
                                   (long) ((_task)->pid),         \
                                   (_task)->signal);              \
                    break;                                        \
            }                                                     \
        }                                                         \
        task_action_dispatch (_task);                             \
        if (__pidafter__ && status_file != NULL)                  \
            _write_status ("%li\n", (long) ((_task)->pid));       \
    } while (0)


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

#if defined(__UCLIBC__)
#include <sys/sysinfo.h>
static int getloadavg(double *a, int n)
{
    struct sysinfo si;
    if (n <= 0) return n ? -1 : 0;
    if (n > 3) n = 3;

    if (sysinfo(&si) != 0) return -1;

    int i = 0;
    for (i=0; i<n; i++)
        a[i] = 1.0/(1<<SI_LOAD_SHIFT) * si.loads[i];

    return n;
}
#endif

static int
reap_and_check (void)
{
    clog_debug("Waiting for a children to reap...");

    int status;
    pid_t pid = waitpid (-1, &status, WNOHANG);

    if (pid == cmd_task.pid) {
        clog_debug("Reaped cmd process %d", pid);

        write_status ("cmd exit %li %i\n", (long) pid, status);

        cmd_task.pid = NO_PID;

        /*
         * If exit-on-success was request AND the process exited ok,
         * then we do not want to respawn, but to gracefully shutdown.
         */
        if (success_exit && WIFEXITED (status) && WEXITSTATUS (status) == 0) {
            clog_debug("cmd process ended successfully, will exit");
            running = 0;
        }
        else if (num_respawns == 0) {
            clog_debug("cmd process respawned max number of times, will exit");
            running = 0;
        } else {
            if (num_respawns > 0) {
                num_respawns -= 1;
            }
            task_action_queue (&cmd_task, A_START);
        }
        return status;
    }
    else if (log_enabled && pid == log_task.pid) {
        clog_debug("Reaped log process %i", pid);

        write_status ("log exit %li %i\n", (long) pid, status);

        log_task.pid = NO_PID;
        task_action_queue (&log_task, A_START);
    }
    else { 
        clog_debug("Reaped unknown process %i", pid);
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
    clog_debug("Got signal %i, %s", signum, signal_to_name(signum));

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
        write_status ("cmd timeout %li\n", (long) cmd_task.pid);
        task_action (&cmd_task, A_STOP);
        task_action_queue (&cmd_task, A_START);
        alarm (cmd_timeout);
        return;
    }

    unsigned i = 0;
    while (forward_signals[i].code != NO_SIGNAL) {
        if (signum == forward_signals[i++].code)
            break;
    }

    if (signum != NO_SIGNAL) {
        /* Try to forward signals */
        if (cmd_signals) {
            clog_debug("Delayed signal %i for cmd process", signum);
            task_action_queue (&cmd_task, A_SIGNAL);
            task_signal_queue (&cmd_task, signum);
        }
        if (log_signals && log_enabled) {
            clog_debug("Delayed signal %i for log process", signum);
            task_action_queue (&log_task, A_SIGNAL);
            task_signal_queue (&log_task, signum);
        }
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


static enum cflag_status
_environ_option(const struct cflag *spec, const char *arg)
{
    if (!spec)
        return CFLAG_NEEDS_ARG;

    const char *equalsign;
    if ((equalsign = strchr(arg, '=')) == NULL) {
        unsetenv(arg);
    } else {
        char *varname = strndup(arg, equalsign - arg);
        setenv(varname, equalsign + 1, 1);
        free(varname);
    }
    return CFLAG_OK;
}


static enum cflag_status
_rlimit_option(const struct cflag *spec, const char *arg)
{
    if (!spec)
        return CFLAG_NEEDS_ARG;

    long value;
    int limit;
    int status = parse_limit_arg(arg, &limit, &value);

    if (status < 0)
        return CFLAG_OK;
    if (status)
        return CFLAG_BAD_FORMAT;

    safe_setrlimit(limit, value);
    return CFLAG_OK;
}


static enum cflag_status
_store_uidgids_option (const struct cflag *spec, const char *arg)
{
    if (!spec)
        return CFLAG_NEEDS_ARG;

    char *arg_copy = strdup(arg);
    int status = parse_uidgids(arg_copy, spec->data);
    free(arg_copy);

    return status ? CFLAG_BAD_FORMAT : CFLAG_OK;
}

static enum cflag_status
_config_option(const struct cflag *spec, const char *arg)
{
    (void) arg;

    if (!spec)
        return CFLAG_NEEDS_ARG;

    assert(!"Unreachable");
    clog_error("Option --config/-C must be the first one specified");
    return CFLAG_BAD_FORMAT;
}

static const struct cflag dmon_options[] = {
    {
        .name = "config", .letter = 'C',
        .func = _config_option,
        .help =
            "Read options from the specified configuration file. If given, "
            "this option must be the first one in the command line.",
    },
    CFLAG(bool, "no-daemon", 'n', &nodaemon,
          "Do not daemonize, stay in foreground."),
    CFLAG(bool, "stderr-redir", 'e', &cmd_task.redir_errfd,
          "Redirect command's standard error stream to its standard "
          "output stream."),
    CFLAG(bool, "cmd-sigs", 's', &cmd_signals,
          "Forward signals to command process."),
    CFLAG(bool, "log-sigs", 'S', &log_signals,
          "Forward signals to log process."),
    CFLAG(bool, "once", '1', &success_exit,
          "Exit if command exits with a zero return code. The process "
          "will be still respawned when it exits with a non-zero code."),
    CFLAG(int, "max-respawns", 'm', &num_respawns,
          "Exit after max number of respawns no matter the exit code."),
    CFLAG(string, "write-info", 'I', &status_path,
          "Write information on process status to the given file. "
          "Sockets and FIFOs may be used."),
    CFLAG(string, "pid-file", 'p', &pidfile_path,
          "Write PID to a file in the given path."),
    CFLAG(string, "work-dir", 'W', &workdir_path,
          "Specify a working directory. All other specified relative paths "
          "have to be specified in relation with this directory."),
    CFLAG(float, "load-high", 'L', &load_high,
          "Stop process when system load surpasses the given value."),
    CFLAG(float, "load-low", 'l', &load_low,
          "Resume process execution when system load drops below the "
          "given value. If not given, defaults to half the value passed "
          "to '-L'."),
    CFLAG(timei, "timeout", 't', &cmd_timeout,
          "If command execution takes longer than the time specified "
          "the process will be killed and started again."),
    CFLAG(timei, "interval", 'i', &cmd_interval,
          "Time to wait between successful command executions. When "
          "exit code is non-zero, the interval is ignored and the "
          "command is executed again as soon as possible."),
    {
        .name = "environ", .letter = 'E',
        .func = _environ_option,
        .help =
            "Define an environment variable, or if no value is given, "
            "delete it. This option can be specified multiple times.",
    },
    {
        .name = "limit", .letter = 'r',
        .func = _rlimit_option,
        .help =
            "Sets a resource limit, given as 'name=value'. This option "
            "can be specified multiple times. Use '-r help' for a list.",
    },
    {
        .name = "cmd-user", .letter = 'u',
        .func = _store_uidgids_option,
        .data = &cmd_task.user,
        .help =
            "User and (optionally) groups to run the command as. Format "
            "is 'user[:group1[:group2[:...groupN]]]'.",
    },
    {
        .name = "log-user", .letter = 'U',
        .func = _store_uidgids_option,
        .data = &log_task.user,
        .help =
            "User and (optionally) groups to run the log process as. "
            "Format is 'user[:group1[:group2[:...groupN]]]'.",
    },
    CFLAG_HELP,
    CFLAG_END
};


int
dmon_main (int argc, char **argv)
{
    clog_init(NULL);

    FILE *pid_file = NULL;
    char *opts_env = NULL;

    /* Check for -C/--config given as first command line argument. */
    if (argc > 2 && ((argv[1][0] == '-' &&
                      argv[1][1] == 'C' &&
                      argv[1][2] == '\0') ||
                     !strcmp("--config", argv[1]))) {
        FILE *input = fopen(argv[2], "r");
        if (!input)
            die("%s: Cannot open file '%s', %s\n", argv[0], argv[2], ERRSTR);

        struct dbuf errmsg = DBUF_INIT;
        if (!conf_parse(input, dmon_options + 1, &errmsg))
            die("%s: Error parsing %s:%s\n", argv[0], argv[2], dbuf_str(&errmsg));

        replace_args_shift(2, &argc, &argv);
    }

    if ((opts_env = getenv ("DMON_OPTIONS")) != NULL)
        replace_args_string (opts_env, &argc, &argv);

    const char *argv0 = cflag_apply(dmon_options,
                                    "cmd [cmd-options] [ -- "
                                    "log-cmd [log-cmd-options]]",
                                    &argc, &argv);

    if (workdir_path) {
        if (chdir (workdir_path) != 0)
            die ("%s: Cannot use '%s' as work directory, %s\n", argv0, workdir_path, ERRSTR);
    }

    if (status_path) {
        int fd = open (status_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd < 0)
            die ("%s: Cannot open '%s' for writing, %s\n", argv0, status_path, ERRSTR);
        status_file = fdopen (fd, "w");
        setvbuf (status_file, NULL, _IOLBF, 0);
    }

    if (cmd_interval && success_exit)
        die ("%s: Options '-i' and '-1' cannot be used together.\n", argv0);

    if (load_enabled && almost_zerof (load_low))
        load_low = load_high / 2.0f;

    cmd_task.argv = argv;

    /* Skip over until "--" is found */
    unsigned i = 0;
    while (i < (unsigned) argc && strcmp (argv[i], "--") != 0) {
        cmd_task.argc++;
        i++;
    }

    /* There is a log command */
    if (i < (unsigned) argc && strcmp (argv[i], "--") == 0) {
        log_task.argc = argc - cmd_task.argc - 1;
        log_task.argv = argv + argc - log_task.argc;
        log_task.argv[log_task.argc] = NULL;
    }

    cmd_task.argv[cmd_task.argc] = NULL;

    if (log_task.argc > 0) {
        if (pipe (log_fds) != 0) {
            die ("%s: Cannot create pipe: %s\n", argv0, ERRSTR);
        }
        clog_debug("pipe_read = %i, pipe_write = %i\n", log_fds[0], log_fds[1]);
        fd_cloexec (log_fds[0]);
        fd_cloexec (log_fds[1]);
    }

    if (clog_debug_enabled) {
        char **xxargv = cmd_task.argv;
        fputs("cmd:", stderr);
        while (*xxargv) {
            fputc(' ', stderr);
            fputs(*xxargv++, stderr);
        }
        fputc('\n', stderr);
        if (log_enabled) {
            char **xxargv = log_task.argv;
            fputs("log:", stderr);
            while (*xxargv) {
                fputc(' ', stderr);
                fputs(*xxargv++, stderr);
            }
            fputc('\n', stderr);
        }
    }

    if (cmd_task.argc == 0)
        die ("%s: No command to run given.\n", argv0);

    if (pidfile_path) {
        int fd = open (pidfile_path, O_TRUNC | O_CREAT | O_WRONLY, 0666);
        if (fd < 0) {
            die ("%s: cannot open '%s' for writing, %s\n",
                 argv0, pidfile_path, ERRSTR);
        }
        pid_file = fdopen (fd, "w");
    }

    if (!nodaemon)
        become_daemon ();

    /* We have a valid file descriptor: write PID */
    if (pid_file) {
        if (fprintf (pid_file, "%li\n", (long) getpid ()) < 0)
            clog_warning("Writing to PID file: %s", ERRSTR);
        fclose (pid_file);
        pid_file = NULL;
    }

    setup_signals ();
    alarm (cmd_timeout);

    cmd_task.write_fd = log_fds[1];
    log_task.read_fd  = log_fds[0];

    int retcode = 0;
    while (running) {
        clog_debug(">>> loop iteration");
        if (check_child) {
            retcode = reap_and_check ();
            clog_debug("retcode = %d", retcode);

            /*
             * Wait the specified timeout but DO NOT use safe_sleep(): here
             * we want an interruptible sleep-wait so reaction to signals is
             * quick, which we definitely want for SIGINT/SIGTERM.
             */
            if (cmd_interval && !success_exit && retcode == 0 && num_respawns != 0) {
                int retval;
                struct timespec ts;
                ts.tv_sec = cmd_interval;
                ts.tv_nsec = 0;

                do {
                    retval = nanosleep (&ts, &ts);
                    clog_debug("nanosleep -> %i\n", retval);
                } while (retval == -1 && errno == EINTR && running);
            }

            /*
             * Either handling signals which interrupt the previous loop,
             * or reap_and_check() may request stopping on successful exit
             */
            if (!running) {
                task_action_queue (&cmd_task, A_NONE);
                break;
            }
        }

        task_action_dispatch_and_write_status ("cmd", &cmd_task);
        if (log_enabled)
            task_action_dispatch_and_write_status ("log", &log_task);

        if (load_enabled) {
            double load_cur;

            clog_debug("Checking load after sleeping 1s");
            interruptible_sleep (1);

            if (getloadavg (&load_cur, 1) == -1)
                clog_debug("getloadavg() failed: %s", ERRSTR);

            if (paused) {
                /* If the current load dropped below load_low -> resume */
                if (load_cur <= load_low) {
                    clog_debug("Resuming...");
                    task_signal (&cmd_task, SIGCONT);
                    write_status ("cmd resume %li\n", (long) cmd_task.pid);
                    paused = 0;
                }
            }
            else {
                /* If the load went above load_high -> pause */
                if (load_cur > load_high) {
                    clog_debug("Pausing...");
                    task_signal (&cmd_task, SIGSTOP);
                    write_status ("cmd pause %li\n", (long) cmd_task.pid);
                    paused = 1;
                }
            }
        }
        else {
            /* Wait for signals to arrive. */
            clog_debug("Waiting for signals to come...");
            pause ();
        }
    }

    clog_debug("Exiting gracefully...");

    if (cmd_task.pid != NO_PID) {
        write_status ("cmd stop %li\n", (long) cmd_task.pid);
        task_action (&cmd_task, A_STOP);
    }
    if (log_enabled && log_task.pid != NO_PID) {
        write_status ("log stop %li\n", (long) log_task.pid);
        task_action (&log_task, A_STOP);
    }

    if (status_file) {
        fclose (status_file);
        status_file = NULL;
    }

    if (WIFEXITED (retcode))
        exit (WEXITSTATUS (retcode));

    exit (EXIT_FAILURE);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
