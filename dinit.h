/*
 * dinit.h
 * Copyright (C) 2011 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __dinit_h__
#define __dinit_h__

/* dinit-util.c */
int  dinit_do_mounts (void);
void dinit_poweroff (void);
void dinit_reboot (void);
void dinit_halt (void);

#endif /* !__dinit_h__ */

