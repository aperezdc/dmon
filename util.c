/*
 * util.c
 * Copyright (C) 2010-2020 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define _GNU_SOURCE 1

#include "deps/cflag/cflag.h"
#include "util.h"
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>
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

#ifdef __GLIBC_PREREQ
# if __GLIBC_PREREQ(2, 29)
#  define USE_LIBC_REALLOCARRAY 1
# endif
#endif

#ifndef USE_LIBC_REALLOCARRAY
# define USE_LIBC_REALLOCARRAY 0
#endif

#if !USE_LIBC_REALLOCARRAY
static void* util_reallocarray(void*, size_t, size_t);
#define reallocarray util_reallocarray
#define DEF_WEAK(dummy)
#include "fallback/reallocarray.c"
#endif /* !LIBC_HAS_REALLOCARRAY */

void
fd_cloexec (int fd)
{
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        die ("unable to set FD_CLOEXEC\n");
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
        die ("could not set handler for signal %s (%i): %s\n",
             name, signum, ERRSTR);
    }
}


void
safe_setrlimit (int what, long value)
{
    struct rlimit r;

    if (getrlimit (what, &r) < 0)
        die ("getrlimit (%s) failed: %s\n", limit_name (what), ERRSTR);

    if (value < 0 || (unsigned long) value > r.rlim_max)
        r.rlim_cur = r.rlim_max;
    else
        r.rlim_cur = value;

    if (setrlimit (what, &r) < 0)
        die ("setrlimit (%s=%li) failed: %s\n", limit_name (what), value, ERRSTR);
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
        fprintf (stderr,
                 "More than %d groups given, ignoring additional ones\n",
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
        die ("cannot daemonize, unable to open '/dev/null': $E\n");

    fd_cloexec (nullfd);

    if (dup2 (nullfd, STDIN_FILENO) < 0)
        die ("cannot daemonize, unable to redirect stdin: %s\n", ERRSTR);
    if (dup2 (nullfd, STDOUT_FILENO) < 0)
        die ("cannot daemonize, unable to redirect stdout: %s\n", ERRSTR);
    if (dup2 (nullfd, STDERR_FILENO) < 0)
        die ("cannot daemonize, unable to redirect stderr: %s\n", ERRSTR);

    pid = fork ();

    if (pid < 0) die ("cannot daemonize: %s\n", ERRSTR);
    if (pid > 0) _exit (EXIT_SUCCESS);

    if (setsid () == -1)
        _exit (111);
}


static bool
_parse_limit_time(const char *sval, long *rval)
{
    assert(sval != NULL);
    assert(rval != NULL);

    unsigned long long val;
    const struct cflag spec = { .data = &val };
    const bool failed = cflag_timei(&spec, sval) != CFLAG_OK;
    *rval = val;
    return failed || (val > LONG_MAX);
}


static bool
_parse_limit_number(const char *sval, long *rval)
{
    assert(sval != NULL);
    assert(rval != NULL);
    return !(sscanf(sval, "%li", rval) == 1);
}


static bool
_parse_limit_bytes (const char *sval, long *rval)
{
    assert(sval != NULL);
    assert(rval != NULL);

    unsigned long long val;
    const struct cflag spec = { .data = &val };
    const bool failed = cflag_bytes(&spec, sval) != CFLAG_OK;
    *rval = val;
    return failed || (val > LONG_MAX);
}


static const struct {
    const char *name;
    int         what;
    bool      (*parse)(const char*, long*);
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
parse_limit_arg(const char *str, int *what, long *value)
{
    assert(str != NULL);
    assert(what != NULL);
    assert(value != NULL);

    const unsigned n_rlimit_specs = sizeof(rlimit_specs) / sizeof(rlimit_specs[0]);

    if (!strcmp(str, "help")) {
        for (unsigned i = 0; i < n_rlimit_specs; i++)
            printf("%s -- %s\n", rlimit_specs[i].name, rlimit_specs[i].desc);
        return -1;
    }

    for (unsigned i = 0; i < n_rlimit_specs; i++) {
        unsigned nlen = strlen(rlimit_specs[i].name);
        if (!strncmp(str, rlimit_specs[i].name, nlen) && str[nlen] == '=') {
            *what = rlimit_specs[i].what;
            return ((*rlimit_specs[i].parse)(str + nlen + 1, value));
        }
    }

    return 1;
}


const char*
limit_name(int what)
{
    const unsigned n_rlimit_specs = sizeof(rlimit_specs) / sizeof(rlimit_specs[0]);

    for (unsigned i = 0; i < n_rlimit_specs; i++) {
        if (what == rlimit_specs[i].what)
            return rlimit_specs[i].name;
    }
    return "<unknown>";
}


#ifndef REPLACE_ARGS_VCHUNK
#define REPLACE_ARGS_VCHUNK 16
#endif /* !REPLACE_ARGS_VCHUNK */

#ifndef REPLACE_ARGS_SCHUNK
#define REPLACE_ARGS_SCHUNK 32
#endif /* !REPLACE_ARGS_SCHUNK */

int
replace_args_string(const char *str, int *pargc, char ***pargv)
{
    assert(str);
    assert(pargc);
    assert(pargv);

    int ch;
    char *s = NULL;
    int maxarg = REPLACE_ARGS_VCHUNK;
    int numarg = 0;
    int quotes = 0;
    int smax = 0;
    int slen = 0;
    char **argv = calloc(maxarg, sizeof(char*));

    /* Copy argv[0] pointer */
    argv[numarg++] = (*pargv)[0];

    while ((ch = *str++) != '\0') {
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
                argv = reallocarray(argv, maxarg, sizeof(char*));
            }

            /* Add terminating "\0" */
            if (slen >= smax) {
                smax += REPLACE_ARGS_SCHUNK;
                s = s ? calloc(smax, sizeof(char))
                      : reallocarray(s, smax, sizeof(char));
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
            if (!isprint (ch)) {
                for (unsigned i = 0; argv[i]; i++)
                    free(argv[i]);
                free(argv);
                if (s) {
                    free(s);
                    s = NULL;
                }
#if   defined(EINVAL)
                errno = EINVAL;
#elif defined(EILSEQ)
                errno = EILSEQ;
#else
#warning Both EINVAL and EILSEQ are undefined, error message will be ambiguous
                errno = 0;
#endif
                return 1;
            }
            if (slen >= smax) {
                smax += REPLACE_ARGS_SCHUNK;
                s = s ? calloc(smax, sizeof(char))
                      : reallocarray(s, smax, sizeof(char));
            }
            s[slen++] = ch;
        }
    }

    /* If there is still an in-progres string, store it. */
    if (slen) {
        /* Add terminating "\0" */
        if (slen >= smax) {
            smax += REPLACE_ARGS_SCHUNK;
            s = reallocarray(s, smax, sizeof(char));
        }

        /* Save string in array. */
        s[slen] = '\0';
        argv[numarg++] = s;
    }

    /* Copy remaining pointers at the tail */
    if ((maxarg - numarg) <= *pargc) {
        maxarg += *pargc;
        argv = reallocarray(argv, maxarg, sizeof(char*));
    }
    for (ch = 1; ch < *pargc; ch++)
        argv[numarg++] = (*pargv)[ch];

    /* Add terminating NULL */
    argv[numarg] = NULL;

    *pargc = numarg;
    *pargv = argv;

    return 0;
}

void
replace_args_shift(unsigned amount,
                   int     *pargc,
                   char  ***pargv)
{
    assert(pargc);
    assert(pargv);
    assert(amount > 0);
    assert(*pargc > (int) amount);

    int    argc = *pargc;
    char **argv = *pargv;
    int    i;

    while (amount--) {
        argc--;
        for (i = 1; i < argc; i++) {
            argv[i] = argv[i+1];
        }
    }
    *pargc = argc;
}

ssize_t
freaduntil(int          fd,
           struct dbuf *buffer,
           struct dbuf *overflow,
           int          delimiter,
           size_t       readbytes)
{
    assert(fd >= 0);
    assert(buffer);
    assert(overflow);

    if (!readbytes) {
        static const size_t default_readbytes = 4096;
        readbytes = default_readbytes;
    }

    for (;;) {
        char *pos = memchr(dbuf_cdata (overflow),
                           delimiter,
                           dbuf_size (overflow));

        if (pos) {
            /*
             * The delimiter has been found in the overflow buffer: remove
             * it from there, and copy the data to the result buffer.
             */
            size_t len = pos - (const char*) dbuf_cdata (overflow) + 1;
            dbuf_addmem(buffer, dbuf_cdata (overflow), len);
            overflow->size -= len;
            memmove (dbuf_data(overflow),
                     dbuf_data(overflow) + len,
                     dbuf_size(overflow));
            return dbuf_size(buffer);
        }

        if (overflow->alloc < (dbuf_size(overflow) + readbytes)) {
            /*
             * XXX Calling dbuf_resize() will *both* resize the buffer data
             * area and set overflow->alloc *and* overflow->size. But we
             * do not want the later to be changed we save and restore it.
             */
            const size_t oldlen = dbuf_size(overflow);
            dbuf_resize(overflow, oldlen + readbytes);
            overflow->size = oldlen;
        }

        ssize_t r = read(fd,
                         dbuf_data(overflow) + dbuf_size(overflow),
                         readbytes);
        if (r > 0) {
            overflow->size += r;
        } else {
            /* Handles both EOF and errors. */
            return r;
        }
    }
}

void
die (const char *format, ...)
{
    va_list al;

    va_start (al, format);
    if (format) {
        vfprintf (stderr, format, al);
        fflush (stderr);
    }
    va_end (al);
    exit (EXIT_FAILURE);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
