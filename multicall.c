/*
 * multicall.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifdef NO_MULTICALL
# error Cannot build __FILE__ with -DNO_MULTICALL
#endif /* NO_MULTICALL */


#include "wheel/wheel.h"
#include <string.h>


typedef int (*mainfunc_t)(int, char**);
#define APPLET_LIST_BEGIN   static mainfunc_t choose_applet (const char *name) {
#define APPLET(_n)          if (!strcmp (name, #_n)) return _n ## _main;
#define APPLET_LIST_END     return NULL; }


extern int dmon_main  (int, char**);
extern int dlog_main  (int, char**);
extern int dslog_main (int, char**);
extern int drlog_main (int, char**);
extern int dinit_main (int, char**);


APPLET_LIST_BEGIN
    APPLET (dmon)
    APPLET (dlog)
    APPLET (dslog)
    APPLET (drlog)
    APPLET (dinit)
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
        w_die ("$s: No such applet in multicall binary.\n", argv0);

    return (*mainfunc)(argc, argv);
}


