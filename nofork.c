/*
 * nofork.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "util.h"


pid_t
fork (void)
{
    return 0;
}


int
daemon (int nochdir, int noclose)
{
    if (!nochdir) {
        if (chdir ("/"))
            return -1;
    }
    if (!noclose) {
        safe_close (0);
        if (safe_openat(AT_FDCWD, "/dev/null", O_RDONLY) == -1)
            return -1;
        safe_close(1);
        if (safe_openat(AT_FDCWD, "/dev/null", O_WRONLY) != 1)
            return -1;
        safe_close(2);
        if (dup (1) != 2)
            return -1;
    }
    return 0;
}


