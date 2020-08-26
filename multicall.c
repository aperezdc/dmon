/*
 * multicall.c
 * Copyright (C) 2010-2020 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#if defined(MULTICALL) && MULTICALL

#include "util.h"
#include <stdlib.h>
#include <string.h>

extern int dlog_main(int, char**);
extern int dmon_main(int, char**);
extern int drlog_main(int, char**);
extern int dslog_main(int, char**);

static const struct {
    const char *name;
    int (*func)(int, char**);
} applets[] = {
    { .name = "dmon", .func = dmon_main },
    { .name = "dlog", .func = dlog_main },
    { .name = "drlog", .func = drlog_main },
    { .name = "dslog", .func = dslog_main },
};


int
main(int argc, char **argv)
{
    const char *argv0 = strrchr(argv[0], '/');
    if (argv0 == NULL)
        argv0 = argv[0];
    else
        argv0++;

    const char *env = getenv("DMON_LIST_MULTICALL_APPLETS");
    const bool list = env && *env != '\0' && *env != '0';

    int (*mainfunc)(int, char**) = NULL;

    for (unsigned i = 0; i < sizeof(applets) / sizeof(applets[0]); i++) {
        if (list) {
            puts(applets[i].name);
        } else if (strcmp(argv0, applets[i].name) == 0) {
            mainfunc = applets[i].func;
            break;
        }
    }

    if (list)
        return EXIT_SUCCESS;

    if (mainfunc == NULL)
        die("%s: No such applet in multicall binary.\n", argv0);

    return (*mainfunc)(argc, argv);
}

#endif /* !MULTICALL */
