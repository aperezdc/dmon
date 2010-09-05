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
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>


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


int
interruptible_sleep (unsigned seconds)
{
    struct timespec ts;

    if (!seconds) return 0;

    ts.tv_sec = seconds;
    ts.tv_nsec = 0;

    return !(nanosleep (&ts, NULL) == -1 && errno == EINTR);
}


int
name_to_uid (const char *str, uid_t *result)
{
    struct passwd *pw;
    unsigned long num;
    char *dummy;

    assert (str);
    assert (result);

    num = strtoul (str, &dummy, 0);
    if (num == ULONG_MAX && errno == ERANGE)
        return 1;

    if (!dummy || *dummy == '\0') {
        *result = (uid_t) num;
        return 0;
    }

    if ((pw = getpwnam (str)) == NULL)
        return 1;

    *result = pw->pw_uid;
    return 0;
}


int
name_to_gid (const char *str, gid_t *result)
{
    struct group *grp;
    unsigned long num;
    char *dummy;

    assert (str);
    assert (result);

    num = strtoul (str, &dummy, 0);

    if (num == ULONG_MAX && errno == ERANGE)
        return 1;

    if (!dummy || *dummy == '\0') {
        *result = (gid_t) num;
        return 0;
    }

    if ((grp = getgrnam (str)) == NULL)
        return 1;


    *result = grp->gr_gid;
    return 0;
}


static int
_parse_gids (char     *s,
             uidgid_t *u)
{
    char *pos = NULL;
    gid_t gid;

    assert (s);
    assert (u);

    if (u->ngid >= DMON_GID_COUNT) {
        format (fd_err,
                "more than @L groups given, ignoring additional ones\n",
                DMON_GID_COUNT);
        return 0;
    }

    pos = strchr (s, ':');
    if (pos != NULL)
        *pos = '\0';

    if (name_to_gid (s, &gid)) {
        if (pos != NULL) *pos = ':';
        return 1;
    }

    if (pos != NULL)
        *pos = ':';

    u->gids[u->ngid++] = gid;

    return (pos == NULL) ? 0 : _parse_gids (pos + 1, u);
}


int
parse_uidgids (char     *s,
               uidgid_t *u)
{
    char *pos = NULL;

    assert (s);
    assert (u);

    memset (u, 0x00, sizeof (uidgid_t));

    pos = strchr (s, ':');
    if (pos != NULL)
        *pos = '\0';

    if (name_to_uid (s, &u->uid)) {
        if (pos != NULL) *pos = ':';
        return 1;
    }
    if (name_to_gid (s, &u->gid)) {
        if (pos != NULL) *pos = ':';
        return 1;
    }

    if (pos != NULL)
        *pos = ':';
    else
        return 0;

    return (pos == NULL) ? 0 : _parse_gids (pos + 1, u);
}


/* vim: expandtab shiftwidth=4 tabstop=4
 */
