/*
 * util.h
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __util_h__
#define __util_h__

void fd_cloexec (int);
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
