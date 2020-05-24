/*
 * dlog.c
 * Copyright (C) 2010-2020 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "wheel/wheel.h"
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

#ifdef NO_MULTICALL
# define dlog_main main
#endif /* NO_MULTICALL */

#ifndef TSTAMP_FMT
#define TSTAMP_FMT "%Y-%m-%d/%H:%M:%S"
#endif /* !TSTAMP_FMT */

#ifndef TSTAMP_LEN
#define TSTAMP_LEN (5 + 3 + 3 + 3 + 3 + 3)
#endif /* !TSTAMP_LEN */


static bool  timestamp = false;
static bool  buffered  = false;
static char *prefix    = NULL;
static int   log_fd    = -1;
static int   in_fd     = STDIN_FILENO;


static const w_opt_t dlog_options[] = {
    { 1, 'p', "prefix", W_OPT_STRING, &prefix,
        "Insert the given prefix string between timestamps and logged text." },

    { 1, 'i', "input-fd", W_OPT_INT, &in_fd,
        "File descriptor to read input from (default: stdin)." },

    { 0, 'b', "buffered", W_OPT_BOOL, &buffered,
        "Buffered operation, do not use flush to disk after each line." },

    { 0, 't', "timestamp", W_OPT_BOOL, &timestamp,
        "Prepend a timestamp in YYYY-MM-DD/HH:MM:SS format to each line." },

    W_OPT_END
};


static void
handle_signal (int signum)
{
    if (log_fd >= 0) {
        if (fsync (log_fd) != 0)
            W_WARN ("Error flushing log: $E\n");

        if (log_fd != STDOUT_FILENO && log_fd != STDERR_FILENO) {
            if (close (log_fd) != 0)
                W_WARN ("Error closing log: $E\n");
            log_fd = -1;
        }
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
    w_buf_t overflow = W_BUF;
    w_buf_t linebuf = W_BUF;
    char *env_opts = NULL;
    struct sigaction sa;
    unsigned consumed;

    if ((env_opts = getenv ("DLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    consumed = w_opt_parse (dlog_options, NULL, NULL, "[logfile-path]", argc, argv);

    if (consumed >= (unsigned) argc) {
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
            die ("%s: error reading input: %s\n", argv[0], ERRSTR);

        if (w_buf_size (&linebuf)) {
            struct iovec iov[6];
            int n_iov = 0;

            char timebuf[TSTAMP_LEN+1];
            size_t timebuf_len;
            if (timestamp) {
                time_t now = time(NULL);
                struct tm *time_gm = gmtime (&now);

                if ((timebuf_len = strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm)) == 0)
                    die ("%s: cannot format timestamp: %s\n", argv[0], ERRSTR);

                iov[n_iov++] = iov_from_data (timebuf, timebuf_len);
                iov[n_iov++] = iov_from_literal (" ");
            }

            if (prefix) {
                iov[n_iov++] = iov_from_string (prefix);
                iov[n_iov++] = iov_from_literal (" ");
            }

            iov[n_iov++] = iov_from_buffer (&linebuf);
            iov[n_iov++] = iov_from_literal ("\n");

            assert ((unsigned) n_iov <= (sizeof (iov) / sizeof (iov[0])));

            if (log_fd < 0) {
                if ((log_fd = open (argv[consumed], O_CREAT | O_APPEND | O_WRONLY, 0666)) < 0)
                    die ("%s: cannot open '%s': %s\n", argv[0], argv[consumed], ERRSTR);
            }

            if (writev (log_fd, iov, n_iov) < 0)
                W_WARN ("$s: writing to log failed: $E\n", argv[0]);

            if (!buffered && log_fd != STDOUT_FILENO && log_fd != STDERR_FILENO) {
                if (fsync (log_fd) != 0)
                    W_WARN ("$s: flushing log failed: $E\n", argv[0]);
            }
        }
        w_buf_clear (&linebuf);
    }

    if (close (log_fd) != 0)
        W_WARN ("$s: error closing log file: $E\n", argv[0]);

    exit (EXIT_SUCCESS);
}

