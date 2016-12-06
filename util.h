/*
 * util.h
 * Copyright (C) 2010-2014 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __util_h__
#define __util_h__

#include "wheel/wheel.h"
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

w_opt_status_t time_period_option  (const w_opt_context_t *ctx);
w_opt_status_t storage_size_option (const w_opt_context_t *ctx);

int replace_args_string (const char *str,
                         int        *argc,
                         char     ***argv);

void replace_args_shift (unsigned    amount,
                         int        *pargc,
                         char     ***pargv);

#endif /* !__util_h__ */

/* vim: expandtab shiftwidth=4 tabstop=4
 */
