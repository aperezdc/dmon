/*
 * dlog.c
 * Copyright (C) 2010-2014 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "wheel/wheel.h"
#include "util.h"
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


static bool    timestamp = false;
static bool    buffered  = false;
static char   *prefix    = NULL;
static w_io_t *log_io    = NULL;
static w_io_t *in_io     = NULL;
static int     in_fd     = -1;


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
    if (buffered) {
        if (w_io_failed (w_io_flush (log_io)))
            W_WARN ("Error flushing log: $E\n");
    }

    if (log_io != w_stdout && log_io != w_stderr) {
        if (w_io_failed (w_io_close (log_io)))
            W_WARN ("Error closing log: $E\n");
        log_io = 0;
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
        log_io = w_stdout;
    }
    else {
        W_IO_NORESULT (w_io_close (w_stdout));
    }

    in_io = (in_fd >= 0) ? w_io_unix_open_fd (in_fd) : w_stdin;
    if (in_io == NULL) {
        w_die ("$s: cannot open input: $E\n", argv[0]);
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
        w_io_result_t ret = w_io_read_line (w_stdin, &linebuf, &overflow, 0);

        if (w_io_failed (ret))
            w_die ("$s: error reading input: $E\n", argv[0]);

        if (w_buf_size (&linebuf)) {
            w_io_result_t r;

            if (timestamp) {
                time_t now = time(NULL);
                char timebuf[TSTAMP_LEN+1];
                struct tm *time_gm = gmtime (&now);

                if (strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm) == 0)
                    w_die ("$s: cannot format timestamp: $E\n", argv[0]);

                if (!log_io) {
                    log_io = w_io_unix_open (argv[consumed],
                                             O_CREAT | O_APPEND | O_WRONLY,
                                             0666);
                    if (!log_io)
                        w_die ("$s: cannot open '$s': $E\n", argv[0], argv[consumed]);
                }

                if (prefix) {
                    r = w_io_format (log_io, "$s $s $B\n", timebuf, prefix, &linebuf);
                } else {
                    r = w_io_format (log_io, "$s $B\n", timebuf, &linebuf);
                }

            } else {
                if (!log_io) {
                    log_io = w_io_unix_open (argv[consumed],
                                             O_CREAT | O_APPEND | O_WRONLY,
                                             0666);
                    if (!log_io)
                        w_die ("$s: cannot open '$s': $E\n", argv[0], argv[consumed]);
                }

                if (prefix) {
                    r = w_io_format (log_io, "$s $B\n", prefix, &linebuf);
                } else {
                    r = w_io_format (log_io, "$B\n", &linebuf);
                }
            }

            if (w_io_failed (r))
                W_WARN ("$s: writing to log failed: $E\n", argv[0]);

            if (!buffered) {
                if (w_io_failed (r = w_io_flush (log_io)))
                    W_WARN ("$s: flushing log failed: $E\n", argv[0]);
            }
        }

        w_buf_clear (&linebuf);

        if (w_io_eof (ret))
            break;
    }

    if (w_io_failed (w_io_close (log_io)))
        W_WARN ("$s: error closing log file: $E\n", argv[0]);

    exit (EXIT_SUCCESS);
}

