/*
 * dslog.c
 * Copyright (C) 2010-2020 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "deps/cflag/cflag.h"
#include "deps/dbuf/dbuf.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#if !(defined(MULTICALL) && MULTICALL)
# define dslog_main main
#endif /* MULTICALL */


#ifndef DEFAULT_FACILITY
#define DEFAULT_FACILITY "daemon"
#endif /* !DEFAULT_FACILITY */


#ifndef DEFAULT_PRIORITY
#define DEFAULT_PRIORITY "warning"
#endif /* !DEFAULT_PRIORITY */


static int
name_to_facility (const char *name)
{
    static const struct {
        const char *name;
        int         code;
    } map[] = {
#ifdef LOG_AUTHPRIV
        { "auth",     LOG_AUTHPRIV },
#else  /* LOG_AUTHPRIV */
        { "auth",     LOG_AUTH     },
#endif /* LOG_AUTHPRIV */
        { "cron",     LOG_CRON     },
        { "daemon",   LOG_DAEMON   },
        { "ftp",      LOG_FTP      },
        { "kern",     LOG_KERN     },
        { "kernel",   LOG_KERN     },
        { "local0",   LOG_LOCAL0   },
        { "local1",   LOG_LOCAL1   },
        { "local2",   LOG_LOCAL2   },
        { "local3",   LOG_LOCAL3   },
        { "local4",   LOG_LOCAL4   },
        { "local5",   LOG_LOCAL5   },
        { "local6",   LOG_LOCAL6   },
        { "local7",   LOG_LOCAL7   },
        { "lpr",      LOG_LPR      },
        { "print",    LOG_LPR      },
        { "printer",  LOG_LPR      },
        { "mail",     LOG_MAIL     },
        { "news",     LOG_NEWS     },
        { "user",     LOG_USER     },
        { "uucp",     LOG_UUCP     },
        { NULL,       0            },
    };
    unsigned i = 0;

    while (map[i].name) {
        if (strcasecmp (name, map[i].name) == 0)
            return map[i].code;
        i++;
    }

    return -1;
}


static int
name_to_priority (const char *name)
{
    static const struct {
        const char *name;
        int         code;
    } map[] = {
        { "emerg",     LOG_EMERG   },
        { "emergency", LOG_EMERG   },
        { "alert",     LOG_ALERT   },
        { "crit",      LOG_CRIT    },
        { "critical",  LOG_CRIT    },
        { "err",       LOG_ERR     },
        { "error",     LOG_ERR     },
        { "warn",      LOG_WARNING },
        { "warning",   LOG_WARNING },
        { "notice",    LOG_NOTICE  },
        { "info",      LOG_INFO    },
        { "debug",     LOG_DEBUG   },
        { NULL,        0           },
    };
    unsigned i = 0;

    while (map[i].name) {
        if (strcasecmp (map[i].name, name) == 0)
            return map[i].code;
        i++;
    }

    return -1;
}


static bool running = true;


static enum cflag_status
_facility_option(const struct cflag *spec, const char *arg)
{
    if (!spec)
        return CFLAG_NEEDS_ARG;

    int facility = name_to_facility(arg);
    if (facility == -1)
        return CFLAG_BAD_FORMAT;

    *((int*) spec->data) = facility;
    return CFLAG_OK;
}


static enum cflag_status
_priority_option(const struct cflag *spec, const char *arg)
{
    if (!spec)
        return CFLAG_NEEDS_ARG;

    int priority = name_to_priority(arg);
    if (priority == -1)
        return CFLAG_BAD_FORMAT;

    *((int*) spec->data) = priority;
    return CFLAG_OK;
}


int
dslog_main (int argc, char **argv)
{
    int flags = 0;
    int in_fd = STDIN_FILENO;
    int facility = name_to_facility (DEFAULT_FACILITY);
    int priority = name_to_priority (DEFAULT_PRIORITY);
    bool console = false;
    bool skip_empty = false;
    char *env_opts = NULL;
    struct dbuf linebuf = DBUF_INIT;
    struct dbuf overflow = DBUF_INIT;

    struct cflag dslog_options[] = {
        {
            .name = "facility", .letter = 'f',
            .help = "Log facility (default: daemon).",
            .func = _facility_option,
            .data = &facility,
        },
        {
            .name = "priority", .letter = 'p',
            .help = "Log priority (default: warning).",
            .func = _priority_option,
            .data = &priority,
        },
        CFLAG(int, "input-fd", 'i', &in_fd,
              "File descriptor to read input from (default: stdin)."),
        CFLAG(bool, "console", 'c', &console,
              "Log to console if sending messages to logger fails."),
        CFLAG(bool, "skip-empty", 'e', &skip_empty,
              "Ignore empty lines with no characters."),
        CFLAG_HELP,
        CFLAG_END
    };

    if ((env_opts = getenv ("DSLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    const char *argv0 = cflag_apply(dslog_options, "[options] name", &argc, &argv);

    if (console)
        flags |= LOG_CONS;

    /* We will be no longer using standard output. */
    close (STDOUT_FILENO);

    if (!argc)
        die ("%s: process name not specified.\n", argv0);

    openlog (argv[0], flags, facility);

    while (running) {
        ssize_t bytes = freadline (in_fd, &linebuf, &overflow, 0);
        if (bytes == 0)
            break; /* EOF */

        if (bytes < 0) {
            fprintf (stderr, "%s: error reading input: %s\n", argv0, ERRSTR);
            exit (111);
        }

        if (!skip_empty || dbuf_size(&linebuf) > 1)
            syslog(priority, "%s", dbuf_str(&linebuf));

        dbuf_clear(&linebuf);
    }

    closelog ();

	exit (EXIT_SUCCESS);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
