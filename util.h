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

int parse_time_arg  (const char*, unsigned long*);
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

void replace_args_shift (unsigned    amount,
                         int        *pargc,
                         char     ***pargv);

#endif /* !__util_h__ */

/* vim: expandtab shiftwidth=4 tabstop=4
 */
