/*
 * dslog.c
 * Copyright (C) 2010 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "wheel.h"
#include "util.h"
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#ifdef NO_MULTICALL
# define dslog_main main
#endif /* NO_MULTICALL */


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


static wbool running = W_YES;


static w_opt_status_t
_facility_option (const w_opt_context_t *ctx)
{
    int *facility;

    w_assert (ctx);
    w_assert (ctx->argument);
    w_assert (ctx->argument[0]);
    w_assert (ctx->option);
    w_assert (ctx->option->extra);

    facility = (int*) ctx->option->extra;

    return ((*facility = name_to_facility (ctx->argument[0])) == -1)
         ? W_OPT_BAD_ARG
         : W_OPT_OK;
}


static w_opt_status_t
_priority_option (const w_opt_context_t *ctx)
{
    int *priority;

    w_assert (ctx);
    w_assert (ctx->argument);
    w_assert (ctx->argument[0]);
    w_assert (ctx->option);
    w_assert (ctx->option->extra);

    priority = (int*) ctx->option->extra;

    return ((*priority = name_to_priority (ctx->argument[0])) == -1)
         ? W_OPT_BAD_ARG
         : W_OPT_OK;
}


int
dslog_main (int argc, char **argv)
{
    int flags = 0;
    int in_fd = -1;
    int facility = name_to_facility (DEFAULT_FACILITY);
    int priority = name_to_priority (DEFAULT_PRIORITY);
    wbool console = W_NO;
    char *env_opts = NULL;
    w_io_t *log_io = NULL;
    w_buf_t linebuf = W_BUF;
    w_buf_t overflow = W_BUF;
    unsigned consumed;

    w_opt_t dslog_options[] = {
        { 1, 'f', "facility", _facility_option, &facility,
            "Log facility (default: daemon)." },
        { 1, 'p', "priority", _priority_option, &priority,
            "Log priority (default: warning)." },
        { 1, 'i', "input-fd", W_OPT_INT, &in_fd,
            "File descriptor to read input from (default: stdin)." },
        { 0, 'c', "console", W_OPT_BOOL, &console,
            "Log to console if sending messages to logger fails." },
        W_OPT_END
    };


    if ((env_opts = getenv ("DSLOG_OPTIONS")) != NULL)
        replace_args_string (env_opts, &argc, &argv);

    consumed = w_opt_parse (dslog_options, NULL, NULL, "name", argc, argv);

    if (console)
        flags |= LOG_CONS;

    log_io = (in_fd >= 0) ? w_io_unix_open_fd (in_fd) : w_stdin;
    if (!log_io)
        w_die ("$s: cannot open input: $E\n", argv[0]);

    /* We will be no longer using standard output. */
    w_io_close (w_stdout);

    if (consumed >= (unsigned) argc)
        w_die ("$s: process name not specified.\n", argv[0]);

    openlog (argv[consumed], flags, facility);

    while (running) {
        ssize_t ret = w_io_read_line (log_io, &linebuf, &overflow, 0);

        if (ret == W_IO_ERR) {
            w_io_format (w_stderr, "$s: error reading input: $E\n", argv[0]);
            exit (111);
        }

        if (w_buf_length (&linebuf))
            syslog (priority, "%s", w_buf_str (&linebuf));

        w_buf_reset (&linebuf);

        if (ret == W_IO_EOF) /* EOF reached */
            break;
    }

    closelog ();

	exit (EXIT_SUCCESS);
}

/* vim: expandtab shiftwidth=4 tabstop=4
 */
