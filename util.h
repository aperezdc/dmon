/*
 * util.h
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __util_h__
#define __util_h__

#include <sys/types.h>

int name_to_uid (const char*, uid_t*);
int name_to_gid (const char*, gid_t*);

void fd_cloexec (int);
void safe_sleep (unsigned);
void die (const char*, ...);

void __dprintf (const char*, ...);

#ifdef DEBUG_TRACE
# define dprint(_x)  __dprintf _x
#else
# define dprint(_x)  ((void)0)
#endif /* DEBUG_TRACE */

#endif /* !__util_h__ */

/* vim: expandtab shiftwidth=4 tabstop=4
 */
