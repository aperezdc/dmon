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

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
# define NODISCARD [[nodiscard]]
# define NORETURN [[noreturn]]
#else
# define NODISCARD __attribute__((warn_unused_result))
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define NORETURN _Noreturn
# else
#  define NORETURN __attribute__((noreturn))
# endif
#endif

#include "deps/dbuf/dbuf.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>

#ifdef __GLIBC_PREREQ
# if __GLIBC_PREREQ(2, 29)
#  define USE_LIBC_REALLOCARRAY 1
# endif
#endif

#ifndef USE_LIBC_REALLOCARRAY
# define USE_LIBC_REALLOCARRAY 0
#endif

#if !USE_LIBC_REALLOCARRAY
void* util_reallocarray(void*, size_t, size_t);
#define reallocarray util_reallocarray
#endif /* !LIBC_HAS_REALLOCARRAY */

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

int safe_close(int fd);
int safe_fsync(int fd);
NODISCARD int safe_openat(int fd, const char*, int flags);
NODISCARD int safe_openatm(int fd, const char*, int flags, mode_t);
NODISCARD ssize_t safe_read(int fd, void*, size_t);
NODISCARD ssize_t safe_writev(int fd, const struct iovec*, int iovcnt);
NODISCARD FILE* safe_fdopen(int fd, const char *mode);
void safe_sleep (unsigned);
void safe_sigaction (const char*, int, struct sigaction*);
void safe_setrlimit (int what, long value);

void fd_cloexec (int);
void become_daemon (void);
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

NORETURN void errexit(int code, const char *format, ...)
    __attribute__((format (printf, 2, 3)));

NORETURN void die(const char *format, ...)
    __attribute__((format (printf, 1, 2)));

NORETURN void fatal(const char *format, ...)
    __attribute__((format (printf, 1, 2)));

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
