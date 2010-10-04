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
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
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


void
safe_setrlimit (int what, long value)
{
    struct rlimit r;

    if (getrlimit (what, &r) < 0)
        die ("getrlimit (@c) failed: @E", limit_name (what));

    if (value < 0 || (unsigned long) value > r.rlim_max)
        r.rlim_cur = r.rlim_max;
    else
        r.rlim_cur = value;

    if (setrlimit (what, &r) < 0)
        die ("setrlimit (@c=@l) failed: @E", limit_name (what), value);
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



static int
_parse_limit_bytes (const char *sval, long *rval)
{
    char *endpos;
    long val;

    assert (sval != NULL);
    assert (rval != NULL);

    val = strtoul (sval, &endpos, 0);
    switch (*endpos) {
        case  'g': case 'G': val *= 1024 * 1024 * 1024; break; /* gigabytes */
        case  'm': case 'M': val *= 1024 * 1024;        break; /* megabytes */
        case  'k': case 'K': val *= 1024;               break; /* kilobytes */
        case '\0': break;
        default  : return 1;
    }
    *rval = val;
    return 0;
}


static int
_parse_limit_time (const char *sval, long *rval)
{
    unsigned long val;
    int retcode;

    assert (sval != NULL);
    assert (rval != NULL);

    /* reuse parse_time_arg() */
    retcode = parse_time_arg (sval, &val);
    *rval = val;
    return retcode;
}


static int
_parse_limit_number (const char *sval, long *rval)
{
    assert (sval != NULL);
    assert (rval != NULL);
    return !(sscanf (sval, "%li", rval) == 1);
}


static const struct {
    const char *name;
    int         what;
    int       (*parse)(const char*, long*);
    const char *desc;
} rlimit_specs[] = {
#ifdef RLIMIT_AS
    { "vmem",  RLIMIT_AS, _parse_limit_bytes,
      "Maximum size of process' virtual memory (bytes)" },
#endif /* RLIMIT_AS */
#ifdef RLIMIT_CORE
    { "core",  RLIMIT_CORE, _parse_limit_bytes,
      "Maximum size of core file (bytes)" },
#endif /* RLIMIT_CORE */
#ifdef RLIMIT_CPU
    { "cpu",   RLIMIT_CPU, _parse_limit_time,
      "Maximum CPU time used (seconds)" },
#endif /* RLIMIT_CPU */
#ifdef RLIMIT_DATA
    { "data",  RLIMIT_DATA, _parse_limit_bytes,
      "Maximum size of data segment (bytes)" },
#endif /* RLIMIT_DATA */
#ifdef RLIMIT_FSIZE
    { "fsize", RLIMIT_FSIZE, _parse_limit_bytes,
      "Maximum size of created files (bytes)" },
#endif /* RLIMIT_FSIZE */
#ifdef RLIMIT_LOCKS
    { "locks", RLIMIT_LOCKS, _parse_limit_number,
      "Maximum number of locked files" },
#endif /* RLIMIT_LOCKS */
#ifdef RLIMIT_MEMLOCK
    { "mlock", RLIMIT_MEMLOCK, _parse_limit_bytes,
      "Maximum number of bytes locked in RAM (bytes)" },
#endif /* RLIMIT_MEMLOCK */
#ifdef RLIMIT_MSGQUEUE
    { "msgq", RLIMIT_MSGQUEUE, _parse_limit_number,
      "Maximum number of bytes used in message queues (bytes)" },
#endif /* RLIMIT_MSGQUEUE */
#ifdef RLIMIT_NICE
    { "nice", RLIMIT_NICE, _parse_limit_number,
      "Ceiling for the process nice value" },
#endif /* RLIMIT_NICE */
#ifdef RLIMIT_NOFILE
    { "files", RLIMIT_NOFILE, _parse_limit_number,
      "Maximum number of open files" },
#endif /* RLIMIT_NOFILE */
#ifdef RLIMIT_NPROC
    { "nproc", RLIMIT_NPROC, _parse_limit_number,
      "Maximum number of processes" },
#endif /* RLIMIT_NPROC */
#ifdef RLIMIT_RSS
#warning Building support for RLIMIT_RSS, this may not work on Linux 2.6+
    { "rss", RLIMIT_RSS, _parse_limit_number,
      "Maximum number of pages resident in RAM" },
#endif /* RLIMIT_RSS */
#ifdef RLIMIT_RTPRIO
    { "rtprio", RLIMIT_RTPRIO, _parse_limit_number,
      "Ceiling for the real-time priority" },
#endif /* RLIMIT_RTPRIO */
#ifdef RLIMIT_RTTIME
    { "rttime", RLIMIT_RTTIME, _parse_limit_time,
      "Maximum real-time CPU time used (seconds)" },
#endif /* RLIMIT_RTTIME */
#ifdef RLIMIT_SIGPENDING
    { "sigpending", RLIMIT_SIGPENDING, _parse_limit_number,
      "Maximum number of queued signals" },
#endif /* RLIMIT_SIGPENDING */
#ifdef RLIMIT_STACK
    { "stack", RLIMIT_STACK, _parse_limit_bytes,
      "Maximum stack segment size (bytes)" },
#endif /* RLIMIT_STACK */
};


int
parse_limit_arg (const char *str, int *what, long *value)
{
    unsigned i;

    assert (str != NULL);
    assert (what != NULL);
    assert (value != NULL);

    if (!strcmp (str, "help")) {
        for (i = 0; i < length_of (rlimit_specs); i++) {
            format (fd_out, "@c -- @c\n",
                    rlimit_specs[i].name,
                    rlimit_specs[i].desc);
        }
        return -1;
    }

    for (i = 0; i < length_of (rlimit_specs); i++) {
        unsigned nlen = strlen (rlimit_specs[i].name);
        if (!strncmp (str, rlimit_specs[i].name, nlen) && str[nlen] == '=') {
            *what = rlimit_specs[i].what;
            return ((*rlimit_specs[i].parse) (str + nlen + 1, value));
        }
    }

    return 1;
}



const char*
limit_name (int what)
{
    unsigned i;

    for (i = 0; i < length_of (rlimit_specs); i++) {
        if (what == rlimit_specs[i].what)
            return rlimit_specs[i].name;
    }
    return "<unknown>";
}


void*
xxalloc (void *p, size_t sz)
{
    if (sz) {
        if (p) {
            p = realloc (p, sz);
        }
        else {
            p = malloc (sz);
        }
        if (p == NULL) {
            die ("virtual memory exhausted");
        }
    }
    else {
        if (p) {
            free (p);
        }
        p = NULL;
    }
    return p;
}


#ifndef REPLACE_ARGS_VCHUNK
#define REPLACE_ARGS_VCHUNK 16
#endif /* !REPLACE_ARGS_VCHUNK */

#ifndef REPLACE_ARGS_SCHUNK
#define REPLACE_ARGS_SCHUNK 32
#endif /* !REPLACE_ARGS_SCHUNK */

int
replace_args_cb (int   (*getc)(void*),
                 int    *pargc,
                 char ***pargv,
                 void   *udata)
{
    int ch;
    char *s = NULL;
    int maxarg = REPLACE_ARGS_VCHUNK;
    int numarg = 0;
    int quotes = 0;
    int smax = 0;
    int slen = 0;
    char **argv = xalloc (char*, maxarg);

    assert (getc);
    assert (pargc);
    assert (pargv);

    /* Copy argv[0] pointer */
    argv[numarg++] = (*pargv)[0];

    while ((ch = (*getc) (udata)) != EOF) {
        if (!quotes && isspace (ch)) {
            if (!slen) {
                /*
                 * Got spaces not inside a quote, and the current argument
                 * is empty: skip spaces at the left side of an argument.
                 */
                continue;
            }

            /*
             * Not inside quotes, got space: add '\0', split and reset
             */
            if (numarg >= maxarg) {
                maxarg += REPLACE_ARGS_VCHUNK;
                argv = xresize (argv, char*, maxarg);
            }

            /* Add terminating "\0" */
            if (slen >= smax) {
                smax += REPLACE_ARGS_SCHUNK;
                s = xresize (s, char, smax);
            }

            /* Save string in array. */
            s[slen] = '\0';
            argv[numarg++] = s;

            /* Reset stuff */
            smax = slen = 0;
            s = NULL;
            continue;
        }

        /*
         * Got a character which is not an space, or *is* an space inside
         * quotes. When character is the same as used for start quoting,
         * end quoting, or start quoting if it's a quote; otherwise, just
         * store the character.
         */
        if (quotes && quotes == ch) {
            quotes = 0;
        }
        else if (ch == '"' || ch == '\'') {
            quotes = ch;
        }
        else {
            if (slen >= smax) {
                smax += REPLACE_ARGS_SCHUNK;
                s = xresize (s, char, smax);
            }
            s[slen++] = ch;
        }
    }

    /* If there is still an in-progres string, store it. */
    if (slen) {
        /* Add terminating "\0" */
        if (slen >= smax) {
            smax += REPLACE_ARGS_SCHUNK;
            s = xresize (s, char, smax);
        }

        /* Save string in array. */
        s[slen] = '\0';
        argv[numarg++] = s;
    }

    /* Copy remaining pointers at the tail */
    if ((maxarg - numarg) <= *pargc) {
        maxarg += *pargc;
        argv = xresize (argv, char*, maxarg);
    }
    for (ch = 1; ch < *pargc; ch++)
        argv[numarg++] = (*pargv)[ch];

    /* Add terminating NULL */
    argv[numarg] = NULL;

    *pargc = numarg;
    *pargv = argv;

    return 0;
}


static int
fd_getc (void *udata)
{
    int fd = *((int*) udata);
    int rc;
    char ch;

    do {
        rc = read (fd, &ch, 1);
    } while (rc < 0 && (errno == EAGAIN || errno == EINTR));

    return (rc == 0) ? EOF : ch;
}


int
replace_args_fd (int     fd,
                 int    *pargc,
                 char ***pargv)
{
    assert (fd >= 0);
    assert (pargc);
    assert (pargv);

    return replace_args_cb (fd_getc, pargc, pargv, &fd);
}


int
replace_args_file (const char *filename,
                   int        *pargc,
                   char     ***pargv)
{
    int fd, ret;

    assert (filename);
    assert (pargc);
    assert (pargv);

    if ((fd = open (filename, O_RDONLY, 0)) < 0)
        return 1;

    ret = replace_args_fd (fd, pargc, pargv);

    close (fd);
    return ret;
}



static int
string_getc (void *udata)
{
    const char **pstr = udata;
    const char *str = *pstr;
    int ret;

    if (*str == '\0')
        return EOF;

    ret = *str++;
    *pstr = str;
    return ret;
}


int
replace_args_string (const char *str,
                     int        *pargc,
                     char     ***pargv)
{
    assert (str);
    assert (pargc);
    assert (pargv);

    return replace_args_cb (string_getc, pargc, pargv, &str);
}


void
replace_args_shift (unsigned amount,
                    int     *pargc,
                    char  ***pargv)
{
    int    argc = *pargc;
    char **argv = *pargv;
    int    i;

    assert (pargc);
    assert (pargv);
    assert (amount > 0);
    assert (*pargc > (int) amount);

    while (amount--) {
        argc--;
        for (i = 1; i < argc; i++) {
            argv[i] = argv[i+1];
        }
    }
    *pargc = argc;
}


/* vim: expandtab shiftwidth=4 tabstop=4
 */
