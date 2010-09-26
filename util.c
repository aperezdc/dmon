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
#include <signal.h>
#include <stdio.h>
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


void
safe_sigaction (const char *name, int signum, struct sigaction *sa)
{
    if (sigaction (signum, sa, NULL) < 0) {
        die ("could not set handler for signal @c (@i): @E",
             name, signum);
    }
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
name_to_uidgid (const char *str,
                uid_t      *uresult,
                gid_t      *gresult)
{
    struct passwd *pw;
    unsigned long num;
    char *dummy;

    assert (str);
    assert (uresult);
    assert (gresult);

    num = strtoul (str, &dummy, 0);
    if (num == ULONG_MAX && errno == ERANGE)
        return 1;

    if (!dummy || *dummy == '\0') {
        if ((pw = getpwuid ((uid_t) num)) == NULL)
            return 1;
    }
    else {
        if ((pw = getpwnam (str)) == NULL)
            return 1;
    }

    *uresult = pw->pw_uid;
    *gresult = pw->pw_gid;

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

    if (name_to_uidgid (s, &u->uid, &u->gid)) {
        if (pos != NULL) *pos = ':';
        return 1;
    }

    if (pos != NULL)
        *pos = ':';
    else
        return 0;

    return (pos == NULL) ? 0 : _parse_gids (pos + 1, u);
}


void
become_daemon (void)
{
    pid_t pid;
    int nullfd = open ("/dev/null", O_RDWR, 0);

    if (nullfd < 0)
        die ("cannot daemonize, unable to open '/dev/null': @E");

    fd_cloexec (nullfd);

    if (dup2 (nullfd, fd_in) < 0)
        die ("cannot daemonize, unable to redirect stdin: @E");
    if (dup2 (nullfd, fd_out) < 0)
        die ("cannot daemonize, unable to redirect stdout: @E");
    if (dup2 (nullfd, fd_err) < 0)
        die ("cannot daemonize, unable to redirect stderr: @E");

    pid = fork ();

    if (pid < 0) die ("cannot daemonize: @E");
    if (pid > 0) _exit (EXIT_SUCCESS);

    if (setsid () == -1)
        _exit (111);
}


int
parse_time_arg (const char *str, unsigned long *result)
{
    char *endpos = NULL;

    assert (str != NULL);
    assert (result != NULL);

    *result = strtoul (str, &endpos, 0);
    if (endpos == NULL || *endpos == '\0')
        return 0;

    switch (*endpos) {
        case 'w': *result *= 60 * 60 * 24 * 7; break;
        case 'd': *result *= 60 * 60 * 24; break;
        case 'h': *result *= 60 * 60; break;
        case 'm': *result *= 60; break;
        default: return 1;
    }

    return 0;
}


int
parse_float_arg (const char *str, float *result)
{
    assert (str != NULL);
    assert (result != NULL);
    return !(sscanf (str, "%f", result) == 1);
}


/* vim: expandtab shiftwidth=4 tabstop=4
 */
