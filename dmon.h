/*
 * dmon.h
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __dmon_h__
#define __dmon_h__

/* dmon-util.c */
void fd_cloexec (int);
void die (const char*, ...);

void __dprintf (const char*, ...);

#ifdef DEBUG_TRACE
# define dprint(_x)  __dprintf _x
#else
# define dprint(_x)  ((void)0)
#endif /* DEBUG_TRACE */

#endif /* !__dmon_h__ */

