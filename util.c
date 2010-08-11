/*
 * util.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

/* Needed for nanosleep(2) */
#define _POSIX_C_SOURCE 199309L

/* Needed for fsync(2) */
#define _BSD_SOURCE 1

#include "util.h"
#include "iolib.h"
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>


void
__dprintf (const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    vformat (fd_err, fmt, args);
    va_end (args);
    fsync (fd_err);
}


void
die (const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    vformat (fd_err, fmt, args);
    format (fd_err, "\n");
    va_end (args);
    fsync (fd_err);

    exit (111);
}


void
fd_cloexec (int fd)
{
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        die ("unable to set FD_CLOEXEC");
}


void
safe_sleep (unsigned seconds)
{
    struct timespec ts;
    int retval;

    /* No time? Save up a syscall! */
    if (!seconds) return;

    ts.tv_sec = seconds;
    ts.tv_nsec = 0;

    do {
        retval = nanosleep (&ts, &ts);
    } while (retval == -1 && errno == EINTR);
}


/* vim: expandtab shiftwidth=4 tabstop=4
 */
