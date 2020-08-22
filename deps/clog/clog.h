/*
 * clog.h
 * Copyright (C) 2020 Adrian Perez de Castro <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef CLOG_H
#define CLOG_H

#include <stdbool.h>
#include <stdio.h>

enum clog_level {
    CLOG_LEVEL_INFO,
    CLOG_LEVEL_WARNING,
    CLOG_LEVEL_ERROR,
    CLOG_LEVEL_DEBUG,
};

extern bool clog_debug_enabled;
extern bool clog_fatal_errors;
extern bool clog_fatal_warnings;

extern void clog_init(const char *env_prefix);
extern void clog_format(enum clog_level,
                        const char *file,
                        unsigned line,
                        const char *func,
                        const char *fmt,
                        ...);

#define clog_message(fmt, ...) \
    fprintf(stderr, "    ** " fmt "\n", ##__VA_ARGS__)

#define clog_info(fmt, ...)                   \
    clog_format(CLOG_LEVEL_INFO,              \
                __FILE__, __LINE__, __func__, \
                (fmt), ##__VA_ARGS__)

#define clog_warning(fmt, ...)                \
    clog_format(CLOG_LEVEL_WARNING,           \
                __FILE__, __LINE__, __func__, \
                (fmt), ##__VA_ARGS__)

#define clog_error(fmt, ...)                  \
    clog_format(CLOG_LEVEL_ERROR,             \
                __FILE__, __LINE__, __func__, \
                (fmt), ##__VA_ARGS__)

#define clog_debug(fmt, ...)                          \
    do {                                              \
        if (clog_debug_enabled)                       \
            clog_format(CLOG_LEVEL_DEBUG,             \
                        __FILE__, __LINE__, __func__, \
                        (fmt), ##__VA_ARGS__);        \
    } while (false)

#if defined(CLOG_SHORT_MACROS) && CLOG_SHORT_MACROS
#define cmessage clog_message
#define cinfo    clog_info
#define cwarning clog_warning
#define cerror   clog_error
#define cdebug   clog_debug
#endif /* CLOG_SHORT_MACROS */

#endif /* !CLOG_H */
