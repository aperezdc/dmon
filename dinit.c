/*
 * dinit.c
 * Copyright (C) 2011 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "wheel.h"
#include <stdlib.h>

#ifdef NO_MULTICALL
# define dinit_main main
#endif /* NO_MULTICALL */


int
dinit_main (int argc, char **argv)
{
    exit (EXIT_SUCCESS);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
