/*
 * dlog.c
 * Copyright (C) 2010-2020 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "deps/cflag/cflag.h"
#include "deps/clog/clog.h"
#include "deps/dbuf/dbuf.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#if !(defined(MULTICALL) && MULTICALL)
# define dlog_main main
#endif /* MULTICALL */

#ifndef TSTAMP_FMT
#define TSTAMP_FMT "%Y-%m-%d/%H:%M:%S"
#endif /* !TSTAMP_FMT */

#ifndef TSTAMP_LEN
#define TSTAMP_LEN (5 + 3 + 3 + 3 + 3 + 3)
#endif /* !TSTAMP_LEN */


static bool  timestamp  = false;
static bool  skip_empty = false;
static bool  buffered   = false;
static char *prefix     = NULL;
static int   log_fd     = -1;
static int   in_fd      = STDIN_FILENO;


static const struct cflag dlog_options[] = {
    CFLAG(string, "prefix", 'p', &prefix,
        "Insert the given prefix string between timestamps and logged text."),
    CFLAG(int, "input-fd", 'i', &in_fd,
        "File descriptor to read input from (default: stdin)."),
    CFLAG(bool, "buffered", 'b', &buffered,
        "Buffered operation, do not use flush to disk after each line."),
    CFLAG(bool, "timestamp", 't', &timestamp,
        "Prepend a timestamp in YYYY-MM-DD/HH:MM:SS format to each line."),
    CFLAG(bool, "skip-empty", 'e', &skip_empty,
        "Ignore empty lines with no characters."),
    CFLAG_HELP,
    CFLAG_END
};


static void
handle_signal (int signum)
{
    if (log_fd >= 0 && log_fd != STDOUT_FILENO && log_fd != STDERR_FILENO && !isatty(log_fd)) {
        if (fsync (log_fd) != 0)
            clog_warning("Flushing log: %s", strerror(errno));
        if (close (log_fd) != 0)
            clog_warning("Closing log: %s", strerror(errno));
        log_fd = -1;
    }

    /*
     * When handling HUP, we just sync and close the log file; in
     * other cases (INT, TERM) we have to exit gracefully, too.
     */
    if (signum != SIGHUP) {
        exit (EXIT_SUCCESS);
    }
}


int
dlog_main (int argc, char **argv)
{
    clog_init(NULL);

    struct dbuf overflow = DBUF_INIT;
    struct dbuf linebuf = DBUF_INIT;
    char *env_opts = NULL;
    struct sigaction sa;

    if ((env_opts = getenv ("DLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    const char *argv0 = cflag_apply(dlog_options, "[options] [logfile-path]", &argc, &argv);

    if (!argc) {
        log_fd = STDOUT_FILENO;
    }
    else {
        close (STDOUT_FILENO);
    }

    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = handle_signal;
    safe_sigaction ("HUP", SIGHUP, &sa);

    sa.sa_handler = handle_signal;
    safe_sigaction ("INT", SIGINT, &sa);

    sa.sa_handler = handle_signal;
    safe_sigaction ("TERM", SIGTERM, &sa);

    for (;;) {
        ssize_t bytes = freadline (in_fd, &linebuf, &overflow, 0);
        if (bytes == 0)
            break; /* EOF */

        if (bytes < 0)
            die ("%s: error reading input: %s\n", argv0, ERRSTR);

        if (!skip_empty || dbuf_size(&linebuf) > 1) {
            struct iovec iov[5];
            int n_iov = 0;

            char timebuf[TSTAMP_LEN+1];
            size_t timebuf_len;
            if (timestamp) {
                time_t now = time(NULL);
                struct tm *time_gm = gmtime (&now);

                if ((timebuf_len = strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm)) == 0)
                    die ("%s: cannot format timestamp: %s\n", argv0, ERRSTR);

                iov[n_iov++] = iov_from_data (timebuf, timebuf_len);
                iov[n_iov++] = iov_from_literal (" ");
            }

            if (prefix) {
                iov[n_iov++] = iov_from_string (prefix);
                iov[n_iov++] = iov_from_literal (" ");
            }

            iov[n_iov++] = iov_from_buffer (&linebuf);

            assert ((unsigned) n_iov <= (sizeof (iov) / sizeof (iov[0])));

            if (log_fd < 0) {
                if ((log_fd = open (argv[0], O_CREAT | O_APPEND | O_WRONLY, 0666)) < 0)
                    die ("%s: cannot open '%s': %s\n", argv0, argv[0], ERRSTR);
            }

            if (writev (log_fd, iov, n_iov) < 0)
                clog_warning("Writing to log: %s", strerror(errno));

            if (!buffered && log_fd != STDOUT_FILENO && log_fd != STDERR_FILENO && !isatty(log_fd)) {
                if (fsync (log_fd) != 0)
                    clog_warning("Flushing log: %s", strerror(errno));
            }
        }
        dbuf_clear(&linebuf);
    }

    if (log_fd >= 0 && close(log_fd) != 0)
        clog_warning("Closing log: %s", strerror(errno));

    exit (EXIT_SUCCESS);
}

