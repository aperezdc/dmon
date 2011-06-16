/*
 * dlog.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "wheel.h"
#include "util.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifndef MULTICALL
# define dlog_main main
#endif /* !MULTICALL */

#ifndef TSTAMP_FMT
#define TSTAMP_FMT "%Y-%m-%d/%H:%M:%S"
#endif /* !TSTAMP_FMT */

#ifndef TSTAMP_LEN
#define TSTAMP_LEN (5 + 3 + 3 + 3 + 3 + 3)
#endif /* !TSTAMP_LEN */


static wbool running   = W_YES;
static wbool timestamp = W_NO;
static wbool buffered  = W_NO;


static const w_opt_t dlog_options[] = {
    { 0, 'b', "buffered", W_OPT_BOOL, &buffered,
        "Buffered operation, do not use flush to disk after each line." },

    { 0, 't', "timestamp", W_OPT_BOOL, &timestamp,
        "Prepend a timestamp in YYYY-MM-DD/HH:MM:SS format to each line." },

    W_OPT_END
};


int
dlog_main (int argc, char **argv)
{
    w_buf_t overflow = W_BUF;
    w_buf_t linebuf = W_BUF;
    char *env_opts = NULL;
    unsigned consumed;
    w_io_t *log_io;

    if ((env_opts = getenv ("DLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    consumed = w_opt_parse (dlog_options, NULL, NULL, "[logfile-path]", argc, argv);

    if (consumed >= (unsigned) argc) {
        log_io = w_stdout;
    }
    else {
        w_io_close (w_stdout);
        log_io = w_io_unix_open (argv[consumed], O_CREAT | O_APPEND | O_WRONLY, 0666);
        if (!log_io)
            w_die ("$s: cannot open '$s': $E\n", argv[0], argv[consumed]);
    }


    while (running) {
        ssize_t ret = w_io_read_line (w_stdin, &linebuf, &overflow, 0);

        if (ret == W_IO_ERR)
            w_die ("$s: error reading input: $E\n", argv[0]);

        if (w_buf_length (&linebuf)) {
            if (timestamp) {
                time_t now = time(NULL);
                char timebuf[TSTAMP_LEN+1];
                struct tm *time_gm = gmtime (&now);

                if (strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm) == 0)
                    w_die ("$s: cannot format timestamp: $E\n", argv[0]);

                w_io_format (log_io, "$s $B\n", timebuf, &linebuf);
            }
            else {
                w_io_format (log_io, "$B\n", &linebuf);
            }

            if (!buffered) {
                w_io_flush (log_io);
            }
        }

        w_buf_reset (&linebuf);

        if (ret == W_IO_EOF) /* EOF reached */
            break;
    }

    w_io_close (log_io);

    exit (EXIT_SUCCESS);
}

