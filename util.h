/*
 * util.h
 * Copyright (C) 2010-2014 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __util_h__
#define __util_h__

#if !(defined(__GNUC__) && __GNUC__ >= 3)
# define __attribute__(dummy)
#endif

#include "deps/dbuf/dbuf.h"
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#define DMON_GID_COUNT 76

#define ERRSTR (strerror (errno))

struct uidgid_s {
    uid_t    uid;
    gid_t    gid;
    unsigned ngid;
    gid_t    gids[DMON_GID_COUNT];
};
typedef struct uidgid_s uidgid_t;

#define UIDGID { 0, 0, 0, {0} }

/* Forward declaration */
struct sigaction;

/* uid[:gid[:gid...]] */
int parse_uidgids (char*, uidgid_t*);

int name_to_uidgid (const char*, uid_t*, gid_t*);
int name_to_gid (const char*, gid_t*);

void fd_cloexec (int);
void become_daemon (void);
void safe_sleep (unsigned);
void safe_sigaction (const char*, int, struct sigaction*);
void safe_setrlimit (int what, long value);
int  interruptible_sleep (unsigned);
const char* limit_name (int);

int parse_limit_arg (const char *str, int *what, long *value);

bool time_period_to_seconds (const char         *str,
                             unsigned long long *result);
bool storage_size_to_bytes  (const char         *str,
                             unsigned long long *result);

int replace_args_string (const char *str,
                         int        *argc,
                         char     ***argv);

void replace_args_shift(unsigned amount,
                        int     *pargc,
                        char  ***pargv);

ssize_t freaduntil(int          fd,
                   struct dbuf *buffer,
                   struct dbuf *overflow,
                   int          delimiter,
                   size_t       readbytes);

static inline ssize_t
freadline(int          fd,
          struct dbuf *buffer,
          struct dbuf *overflow,
          size_t       readbytes)
{
    return freaduntil(fd, buffer, overflow, '\n', readbytes);
}

void die(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)))
    __attribute__ ((noreturn));

static inline struct iovec
iov_from_data (void *data, size_t size)
{
    return (struct iovec) { .iov_base = data, .iov_len = size };
}

static inline struct iovec
iov_from_buffer (struct dbuf *buffer)
{
    return iov_from_data (dbuf_data(buffer), dbuf_size(buffer));
}

static inline struct iovec
iov_from_string (char *str)
{
    return iov_from_data (str, strlen (str));
}

#define iov_from_literal(_s) \
    (iov_from_data ((_s), (sizeof (_s)) - 1))

#endif /* !__util_h__ */

/* vim: expandtab shiftwidth=4 tabstop=4
 */
