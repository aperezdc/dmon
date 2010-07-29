/*
 * dmon-util.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "dmon.h"
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>


void
__dprintf (const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end (args);
    fflush (stderr);
}


void
die (const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end (args);
    fflush (stderr);

    exit (111);
}


void
fd_cloexec (int fd)
{
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        die ("unable to set FD_CLOEXEC");
}


/* vim: expandtab shiftwidth=4 tabstop=4
 */
