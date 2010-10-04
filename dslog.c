/*
 * dslog.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "util.h"
#include "iolib.h"
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#ifndef MULTICALL
# define dslog_main main
#endif /* !MULTICALL */


#ifndef DEFAULT_FACILITY
#define DEFAULT_FACILITY "daemon"
#endif /* !DEFAULT_FACILITY */


#ifndef DEFAULT_PRIORITY
#define DEFAULT_PRIORITY "warning"
#endif /* !DEFAULT_PRIORITY */


#define _dlog_help_message \
    "Usage: @c [options] name\n" \
    "\n" \
    "  -f FACILITY   Log facility (default: " DEFAULT_FACILITY ")\n" \
    "  -p PRIORITY   Log priority (default: " DEFAULT_PRIORITY ")\n" \
    "  -c            Log to console if sending messages to the logger fails.\n" \
    "  -h, -?        Show this help text.\n" \
    "\n"


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


static int running = 1;


int
dslog_main (int argc, char **argv)
{
    int c;
    int flags = 0;
    int facility = name_to_facility (DEFAULT_FACILITY);
    int priority = name_to_priority (DEFAULT_PRIORITY);
    char *env_opts = NULL;
    buffer linebuf = BUFFER;
    buffer overflow = BUFFER;

    if ((env_opts = getenv ("DSLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    while ((c = getopt (argc, argv, "?hcf:p:")) != -1) {
        switch (c) {
            case 'f':
                if ((facility = name_to_facility (optarg)) == -1) {
                    format (fd_err, "@c: unrecognized facility name '@c'\n",
                            argv[0], optarg);
                    exit (EXIT_FAILURE);
                }
                break;
            case 'p':
                if ((priority = name_to_priority (optarg)) == -1) {
                    format (fd_err, "@c: unrecognized priority name '@c'\n",
                            argv[0], optarg);
                    exit (EXIT_FAILURE);
                }
                break;
            case 'c':
                flags |= LOG_CONS;
                break;
            case 'h':
            case '?':
                format (fd_out, _dlog_help_message, argv[0]);
                exit (EXIT_SUCCESS);
            default:
                format (fd_err, "@c: unrecognized option '@c'\n", argv[0], optarg);
                format (fd_err, _dlog_help_message, argv[0]);
                exit (EXIT_FAILURE);
        }
    }

    /* We will be no longer using standard output. */
    close (fd_out);

    if (optind >= argc) {
        format (fd_err, "@c: process name not specified.\n", argv[0]);
        format (fd_err, _dlog_help_message, argv[0]);
        exit (EXIT_FAILURE);
    }

    openlog (argv[optind], flags, facility);

    while (running) {
        int ret = readlineb (fd_in, &linebuf, 0, &overflow);

        if (ret == -1) {
            format (fd_err, "@c: error reading input: @c\n",
                    argv[0], strerror (errno));
            exit (111);
        }

        if (blength (&linebuf))
            syslog (priority, "%s", bstr (&linebuf));

        bfree (&linebuf);

        if (ret == 0) /* EOF reached */
            break;
    }

    closelog ();

	exit (EXIT_SUCCESS);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
