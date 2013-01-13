/*
 * dinit.h
 * Copyright (C) 2011-2012 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __dinit_h__
#define __dinit_h__

#include "wheel.h"

/* dinit-util.c */

/*
 * Equivalent to doing "head -n 1 path". Stores the first line of
 * the file in the given buffer. Returns whether there was some
 * error while reading the value.
 */
w_bool_t dinit_file_head_n1 (const char *path, w_buf_t *buf);

/*
 * Those are using during system initialization of the system.
 */
int dinit_init_filesystem (void);
int dinit_init_hostname (void);

/*
 * Poweroff/reboot/halt the machine.
 */
void dinit_poweroff (void);
void dinit_reboot (void);
void dinit_halt (void);

/*
 * Prints a panic message, formatted with w_io_formatv, in
 * red, bold, standing-out text. System might be rebooted
 * if DINIT_REBOOT_ON_PANIC is set when building.
 */
void dinit_panic (const char *fmt, ...);

#endif /* !__dinit_h__ */

