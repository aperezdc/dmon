/*
 * util.h
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __util_h__
#define __util_h__

#include <sys/types.h>
#define DMON_GID_COUNT 76

struct uidgid_s {
    uid_t    uid;
    gid_t    gid;
    unsigned ngid;
    gid_t    gids[DMON_GID_COUNT];
};
typedef struct uidgid_s uidgid_t;

#define UIDGID { 0, 0, 0, {0} }

#define length_of(_v)  (sizeof (_v) / sizeof ((_v)[0]))

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
void die (const char*, ...);
const char* limit_name (int);

void __dprintf (const char*, ...);

int parse_time_arg  (const char*, unsigned long*);
int parse_float_arg (const char*, float*);
int parse_limit_arg (const char*, int*, long*);

int replace_args_cb     (int       (*getc)(void*),
                         int        *argc,
                         char     ***argv,
                         void       *udata);

int replace_args_fd     (int         fd,
                         int        *pargc,
                         char     ***pargv);

int replace_args_file   (const char *filename,
                         int        *argc,
                         char     ***argv);

int replace_args_string (const char *str,
                         int        *argc,
                         char     ***argv);


void*   xxalloc      (void *p, size_t sz);
#define xrealloc     xxalloc
#define xmalloc(_s)  xxalloc(NULL, (_s))
#define xfree(_p)    (_p) = xxalloc((_p), 0)

#define xalloc(_t, _n) \
    ((_t *) xxalloc (NULL, (_n) * sizeof (_t)))
#define xresize(_p, _t, _n) \
    ((_t *) xxalloc ((_p), (_n) * sizeof (_t)))

#ifdef DEBUG_TRACE
# define dprint(_x)  __dprintf _x
#else
# define dprint(_x)  ((void)0)
#endif /* DEBUG_TRACE */

#endif /* !__util_h__ */

/* vim: expandtab shiftwidth=4 tabstop=4
 */
