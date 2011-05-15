/*
 * nofork.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>


pid_t
fork (void)
{
    return 0;
}


int
daemon (int nochdir, int noclose)
{
    if (!nochdir == 0) {
        if (chdir ("/"))
            return -1;
    }
    if (!noclose) {
        close (0);
        if (open ("/dev/null", O_RDONLY, 0) != 0)
            return -1;
        close (1);
        if (open ("/dev/null", O_WRONLY, 0) != 1)
            return -1;
        close (2);
        if (dup (1) != 2)
            return -1;
    }
    return 0;
}


