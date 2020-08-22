/*
 * clog.c
 * Copyright (C) 2020 Adrian Perez de Castro <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#define _POSIX_C_SOURCE 1

#include "clog.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool clog_fatal_errors   = true;
bool clog_fatal_warnings = false;
bool clog_debug_enabled  = false;
static bool use_colors   = false;

static const struct {
    const char *name;
    bool *data;
} env_opts[] = {
    { "DEBUG",          &clog_debug_enabled  },
    { "FATAL_ERRORS",   &clog_fatal_errors   },
    { "FATAL_WARNINGS", &clog_fatal_warnings },
    { "COLOR_MESSAGES", &use_colors          },
};

static const char *names_plain[] = {
    [CLOG_LEVEL_INFO]    = "INFO   ",
    [CLOG_LEVEL_WARNING] = "WARN   ",
    [CLOG_LEVEL_ERROR]   = "ERROR  ",
    [CLOG_LEVEL_DEBUG]   = "DEBUG  ",
};
static const char *names_colored[] = {
    [CLOG_LEVEL_INFO]    = "\33[32m" "INFO   " "\33[39m",
    [CLOG_LEVEL_WARNING] = "\33[91m" "WARN   " "\33[39m",
    [CLOG_LEVEL_ERROR]   = "\33[31m" "ERROR  " "\33[39m",
    [CLOG_LEVEL_DEBUG]   = "\33[35m" "DEBUG  " "\33[39m",
};
static const char **names = names_plain;

static const char loc_format_plain[] =
    "  [%s:%d, %s]"
    "\n";
static const char loc_format_colored[] =
    "\33[90m"
    "  [%s:%d, %s]"
    "\33[39m"
    "\n";
static const char *loc_format = loc_format_plain;

void
clog_init(const char *env_prefix)
{
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
    use_colors = isatty(fileno(stderr));

    if (!env_prefix)
        env_prefix = "LOG";

    unsigned env_prefix_len = strlen(env_prefix);
    char varname[env_prefix_len + 16];
    memcpy(varname, env_prefix, env_prefix_len);
    varname[env_prefix_len] = '_';

    for (unsigned i = 0; i < sizeof (env_opts) / sizeof (env_opts[0]); i++) {
        strcpy(varname + env_prefix_len + 1, env_opts[i].name);
        const char *value = getenv(varname);
        if (value)
            *env_opts[i].data = (*value != '\0' && *value != '0');
    }

    if (use_colors) {
        names = names_colored;
        loc_format = loc_format_colored;
    }
}


void
clog_format(enum clog_level level,
            const char *file,
            unsigned line,
            const char *func,
            const char *fmt,
            ...)
{
    fputs(names[level], stderr);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, loc_format, file, line, func);

    if ((level == CLOG_LEVEL_WARNING && clog_fatal_warnings) ||
        (level == CLOG_LEVEL_ERROR && clog_fatal_errors))
    {
        fflush(stderr);
        abort();
    }
}
