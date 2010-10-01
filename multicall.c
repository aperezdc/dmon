/*
 * multicall.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef MULTICALL
# error Cannot build __FILE__ without -DMULTICALL
#endif /* !MULTICALL */


#include "util.h"
#include "iolib.h"
#include <string.h>
#include <assert.h>


typedef int (*mainfunc_t)(int, char**);
#define APPLET_LIST_BEGIN   static mainfunc_t choose_applet (const char *name) {
#define APPLET(_n)          if (!strcmp (name, #_n)) return _n ## _main;
#define APPLET_LIST_END     return NULL; }


extern int dmon_main  (int, char**);
extern int dlog_main  (int, char**);
extern int dslog_main (int, char**);

#ifdef MULTICALL_ROTLOG
extern int rotlog_main(int, char**);
#endif /* MULTICALL_ROTLOG */


APPLET_LIST_BEGIN
    APPLET (dmon)
    APPLET (dlog)
    APPLET (dslog)
#ifdef MULTICALL_ROTLOG
    APPLET (rotlog)
#endif /* MULTICALL_ROTLOG */
APPLET_LIST_END



int
main (int argc, char **argv)
{
    const char *argv0 = strrchr (argv[0], '/');
    mainfunc_t mainfunc;

    if (argv0 == NULL)
        argv0 = argv[0];
    else
        argv0++;

    mainfunc = choose_applet (argv0);

    if (mainfunc == NULL)
        die ("@c: No such applet in multicall binary", argv0);

    return (*mainfunc)(argc, argv);
}


