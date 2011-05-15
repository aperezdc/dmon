/*
 * dlog.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "wheel.h"
#include "util.h"
#include "iolib.h"
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
        "Buffered operation, do not use fsync() after each line." },

    { 0, 't', "timestamp", W_OPT_BOOL, &timestamp,
        "Prepend a timestamp in YYYY-MM-DD/HH:MM:SS format to each line." },

    W_OPT_END
};


int
dlog_main (int argc, char **argv)
{
    buffer overflow = BUFFER;
    buffer linebuf = BUFFER;
    char *env_opts = NULL;
    unsigned consumed;
    int log_fd = -1;

    if ((env_opts = getenv ("DLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    consumed = w_opt_parse (dlog_options, NULL, NULL, argc, argv);

    if (consumed >= (unsigned) argc) {
        log_fd = fd_out;
    }
    else {
        close (fd_out);
        log_fd = open (argv[consumed], O_CREAT | O_APPEND | O_WRONLY, 0666);
        if (log_fd < 0) {
            format (fd_err, "@c: cannot open '@c': @E\n", argv[0], argv[consumed]);
            exit (EXIT_FAILURE);
        }
    }


    while (running) {
        int ret = readlineb (fd_in, &linebuf, 0, &overflow);

        if (ret == -1)
            w_die ("$s: error reading input: $E\n", argv[0]);

        if (blength (&linebuf)) {
            if (timestamp) {
                time_t now = time(NULL);
                char timebuf[TSTAMP_LEN+1];
                struct tm *time_gm = gmtime (&now);

                if (strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm) == 0)
                    w_die ("$s: cannot format timestamp: $E\n", argv[0]);

                format (log_fd, "@c @b\n", timebuf, &linebuf);
            }
            else {
                format (log_fd, "@b\n", &linebuf);
            }

            if (!buffered) {
                fsync (log_fd);
            }
        }

        bfree (&linebuf);

        if (ret == 0) /* EOF reached */
            break;
    }

    close (log_fd);

    exit (EXIT_SUCCESS);
}

