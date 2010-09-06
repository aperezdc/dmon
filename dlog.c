/*
 * dlog.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

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

#define _dlog_help_message                                      \
    "Usage: @c [options] [logfile]\n"                           \
    "Save lines from standard input to a log file or stdout.\n" \
    "\n"                                                        \
    "  -b        Buffered operation, do not use fsync().\n"     \
    "  -c        Do not prepend a timestamp to each line.\n"    \
    "  -h, -?    Show this help message.\n"                     \
    "\n"

#ifndef TSTAMP_FMT
#define TSTAMP_FMT "%Y-%m-%d/%H:%M:%S"
#endif /* !TSTAMP_FMT */

#ifndef TSTAMP_LEN
#define TSTAMP_LEN (5 + 3 + 3 + 3 + 3 + 3)
#endif /* !TSTAMP_LEN */


static int timestamp = 1;
static int running   = 1;
static int buffered  = 0;


int
dlog_main (int argc, char **argv)
{
    buffer overflow = BUFFER;
    buffer linebuf = BUFFER;
    int log_fd = -1;
    int c;

    while ((c = getopt (argc, argv, "?hbc")) != -1) {
        switch (c) {
            case 'b':
                buffered = 1;
                break;
            case 'c':
                timestamp = 0;
                break;
            case '?':
            case 'h':
                format (fd_out, _dlog_help_message, argv[0]);
                exit (EXIT_SUCCESS);
            default:
                format (fd_err, "@c: unrecognized option '@c'\n",
                        argv[0], optarg);
                format (fd_err, _dlog_help_message, argv[0]);
                exit (EXIT_FAILURE);
        }
    }

    /* We will be no longer using standard output. */

    if (optind >= argc) {
        log_fd = fd_out;
    }
    else {
        close (fd_out);
        log_fd = open (argv[optind], O_CREAT | O_APPEND | O_WRONLY, 0666);
        if (log_fd < 0) {
            format (fd_err, "@c: cannot open '@c': @E\n", argv[0], argv[optind]);
            exit (EXIT_FAILURE);
        }
    }


    while (running) {
        int ret = readlineb (fd_in, &linebuf, 0, &overflow);

        if (ret == -1)
            die ("@c: error reading input: @E", argv[0]);

        if (blength (&linebuf)) {
            if (timestamp) {
                time_t now = time(NULL);
                char timebuf[TSTAMP_LEN+1];
                struct tm *time_gm = gmtime (&now);

                if (strftime (timebuf, TSTAMP_LEN+1, TSTAMP_FMT, time_gm) == 0)
                    die ("@c: cannot format timestamp: @E", argv[0]);

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

