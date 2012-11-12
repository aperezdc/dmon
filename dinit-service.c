/*
 * dinit-service.c
 * Copyright (C) 2012 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "dinit-service.h"
#include <limits.h>
#include <stdint.h>
#include <fcntl.h>


void
_service_free (void *ptr)
{
    service_t *service = ptr;
    if (service) {
        w_free (service->name);
    }
}


static void
_service_set_defaults (service_t *s)
{
    s->name      = NULL;
    s->mode      = SERVICE_RESPAWN;
    s->log.mode  = SERVICE_LOG_NONE;
}


typedef struct {
    const char *key;
    intptr_t    val;
} _keyval_t;

#define KEYVAL_START(_v, _n) \
    static const _keyval_t _v [] = { { (_n), 0 },
#define KEYVAL(_k, _v)   { (_k), (intptr_t) (_v) },
#define KEYVAL_END       { NULL, 0 } }


#define PTR_OF(_s, _o, _t) \
    ((_t*) (((char*) (_s)) + (_o)))


KEYVAL_START (_run_modes, "run mode")
    KEYVAL ("once"   , SERVICE_ONCE)
    KEYVAL ("respawn", SERVICE_RESPAWN)
KEYVAL_END;

KEYVAL_START (_log_modes, "log mode")
    KEYVAL ("none"  , SERVICE_LOG_NONE)
    KEYVAL ("append", SERVICE_LOG_APPEND)
    KEYVAL ("file"  , SERVICE_LOG_APPEND)
    KEYVAL ("rotate", SERVICE_LOG_ROTATE)
    KEYVAL ("syslog", SERVICE_LOG_SYSLOG)
KEYVAL_END;

KEYVAL_START (_yesno_opt, "boolean")
    KEYVAL ("true" , W_YES)
    KEYVAL ("yes"  , W_YES)
    KEYVAL ("1"    , W_YES)
    KEYVAL ("false", W_NO)
    KEYVAL ("no"   , W_NO)
    KEYVAL ("0"    , W_NO)
KEYVAL_END;

KEYVAL_START (_log_facilities, "log facility")
KEYVAL_END;

KEYVAL_START (_log_priorities, "log priority")
KEYVAL_END;


static void
_parse_enum_kv (w_parse_t  *p,
                service_t  *s,
                w_bool_t    multiline,
                ptrdiff_t   voffset,
                const void *extra)
{
    char *word = NULL;
    const _keyval_t *kv = extra;
    const char *thing = kv->key;

    w_assert (!multiline);
    w_assert (extra);
    w_unused (multiline);

    if (!(word = w_parse_ident (p)))
        w_parse_error (p, "Identifier expected");

    for (++kv; kv->key; kv++) {
        if (!strcmp (word, kv->key)) {
            int *v = PTR_OF (s, voffset, int);
            *v = (int) kv->val;
            w_free (word);
            return;
        }
    }

    w_parse_ferror (p, "Invalid $s '$s'", thing, word);
    w_free (word);
    w_parse_rerror (p);
}


static void
_parse_string (w_parse_t  *p,
               service_t  *s,
               w_bool_t    multiline,
               ptrdiff_t   voffset,
               const void *extra)
{
    w_buf_t buf = W_BUF;
    char **v = PTR_OF (s, voffset, char*);

    w_assert (!extra);
    w_unused (extra);

    w_unused (multiline);
    w_unused (voffset);
    w_unused (extra);

    if (multiline) {
        /* read all until a lone marker in the beginning of a line */
        int comment_char = p->comment;
        p->comment = 0;

        /* long inputs end up when finding a less-than sign in column one */
        while (p->look != W_IO_EOF && !(p->look == '<' && p->lpos == 1)) {
            w_buf_append_char (&buf, p->look);
            w_parse_getchar (p);
        }
        p->comment = comment_char;
        w_parse_match (p, '<');
        w_parse_match (p, '<');
    }
    else {
        /* read up to the end of line */
        while (p->look != '\n') {
            w_buf_append_char (&buf, p->look);
            w_parse_getchar (p);
        }
    }
    w_parse_skip_ws (p);
    *v = w_buf_str (&buf);
}


static void
_parse_uint (w_parse_t  *p,
             service_t  *s,
             w_bool_t    multiline,
             ptrdiff_t   voffset,
             const void *extra)
{
    int *ivalp = PTR_OF (s, voffset, int);
    long lval;

    w_assert (!multiline);
    w_unused (multiline);
    w_unused (extra);

    if (!w_parse_long (p, &lval))
        w_parse_error (p, "Integer expected");
    if (lval > INT_MAX)
        w_parse_error (p, "Integer value '$l' is too big", lval);
    if (lval < INT_MIN)
        w_parse_error (p, "Integer value '$l' is too low", lval);

    *ivalp = (int) lval;
}


static void
_parse_size (w_parse_t  *p,
             service_t  *s,
             w_bool_t    multiline,
             ptrdiff_t   voffset,
             const void *extra)
{
    char *word = NULL;

    w_assert (!multiline);
    w_unused (multiline);
    w_unused (extra);

    if (!(word = w_parse_word (p)))
        w_parse_error (p, "Data size expected");

    if (w_str_size_bytes (word, PTR_OF (s, voffset, unsigned long long))) {
        w_parse_ferror (p, "Invalid data size '$s'", word);
        w_free (word);
        w_parse_rerror (p);
    }

    w_free (word);
}


static void
_parse_time (w_parse_t  *p,
             service_t  *s,
             w_bool_t    multiline,
             ptrdiff_t   voffset,
             const void *extra)
{
    char *word = NULL;

    w_assert (!multiline);
    w_unused (multiline);
    w_unused (extra);

    if (!(word = w_parse_word (p)))
        w_parse_error (p, "Time period expected");

    if (w_str_time_period (word, PTR_OF (s, voffset, unsigned long long))) {
        w_parse_ferror (p, "Invalid time period '$s'", word);
        w_free (word);
        w_parse_rerror (p);
    }

    w_free (word);
}


static const struct {
    const char *keyword;
    void      (*parse) (w_parse_t*, service_t*, w_bool_t, ptrdiff_t, const void*);
    w_bool_t    multiline;
    ptrdiff_t   voffset;
    const void *extra;
} _parse_functions[] = {
    { "command", _parse_string,  W_YES, w_offsetof (service_t, command), NULL       },
    { "mode"   , _parse_enum_kv, W_NO,  w_offsetof (service_t, mode),    _run_modes },
    { "pidfile", _parse_string,  W_NO,  w_offsetof (service_t, pidfile), NULL       },
    { "enabled", _parse_enum_kv, W_NO,  w_offsetof (service_t, enabled), _yesno_opt },

    { "log",
        _parse_enum_kv, W_NO, w_offsetof (service_t, log.mode), _log_modes },
    { "log.prefix",
        _parse_string,  W_NO, w_offsetof (service_t, log.prefix), NULL },
    { "log.syslog.facilty",
        _parse_enum_kv, W_NO, w_offsetof (service_t, log.syslog.facility), _log_facilities },
    { "log.syslog.priority",
        _parse_enum_kv, W_NO, w_offsetof (service_t, log.syslog.priority), _log_priorities },
    { "log.syslog.console",
        _parse_enum_kv, W_NO, w_offsetof (service_t, log.syslog.console), _yesno_opt },
    { "log.filesystem.path",
        _parse_string,  W_NO, w_offsetof (service_t, log.filesystem.path), NULL },
    { "log.filesystem.buffered",
        _parse_enum_kv, W_NO, w_offsetof (service_t, log.filesystem.buffered), _yesno_opt },
    { "log.filesystem.timestamp",
        _parse_enum_kv, W_NO, w_offsetof (service_t, log.filesystem.timestamp), _yesno_opt },
    { "log.filesystem.rotate.max-files",
        _parse_uint,    W_NO, w_offsetof (service_t, log.filesystem.rotate.max_files), NULL },
    { "log.filesystem.rotate.max-size",
        _parse_size,    W_NO, w_offsetof (service_t, log.filesystem.rotate.max_size), NULL },
    { "log.filesystem.rotate.max-time",
        _parse_time,    W_NO, w_offsetof (service_t, log.filesystem.rotate.max_time), NULL },

    { "script.before",
        _parse_string, W_YES, w_offsetof (service_t, before.script), NULL },
    { "script.before.shell",
        _parse_string, W_NO,  w_offsetof (service_t, before.shell), NULL },
    { "script.after",
        _parse_string, W_YES, w_offsetof (service_t, after.script), NULL },
    { "script.after.shell",
        _parse_string, W_NO,  w_offsetof (service_t, after.shell), NULL },
};


static void
_parse_service (w_parse_t *p, service_t *s)
{
    void (*parse_item) (w_parse_t*, service_t*, w_bool_t, ptrdiff_t, const void*) = NULL;
    w_bool_t multiline = W_NO;
    ptrdiff_t voffset  = 0;
    const void *extra  = NULL;
    char *kw;
    unsigned i;

    w_assert (p);
    w_assert (s);

    _service_set_defaults (s);

    while (p->look != W_IO_EOF) {
        if (!(kw = w_parse_word (p))) {
            w_parse_error (p, "Keyword expected");
        }

        for (i = 0; i < w_lengthof (_parse_functions); i++) {
            if (!strcmp (kw, _parse_functions[i].keyword)) {
                parse_item = _parse_functions[i].parse;
                multiline  = _parse_functions[i].multiline;
                voffset    = _parse_functions[i].voffset;
                extra      = _parse_functions[i].extra;
                break;
            }
        }

        if (!parse_item) {
            w_parse_ferror (p, "Invalid keyword '$s'", kw);
            w_free (kw);
            w_parse_rerror (p);
        }

        if (p->look == '<') {
            /* multiline variable assignment */
            if (!multiline) {
                w_parse_ferror (p, "Multi-line values are not allowed"
                                   " for '$s'", kw);
                w_free (kw);
                w_parse_rerror (p);
            }

            w_free (kw);
            (*parse_item) (p, s, W_YES, voffset, extra);
        }
        else {
            /* single-line assignment */
            w_free (kw);
            w_parse_match (p, '=');
            (*parse_item) (p, s, W_NO, voffset, extra);
        }
    }
}


service_t*
service_parse (const char *name, char **msg)
{
    w_parse_t  parser;
    char      *pmsg    = NULL;
    w_io_t    *input   = NULL;
    service_t *service = w_obj_new (service_t);
    char      *path    = (name[0] != '/')
                       ? w_strfmt (DINIT_SERVICES "/%s", name)
                       : (char*) name;

    if (!(input = w_io_unix_open (path, O_RDONLY, 0))) {
        if (msg) *msg = w_strfmt ("service file '%s' cannot be"
                                  " opened for reading", path);
        goto _return;
    }

    w_parse_run (&parser, input, '#',
                 (w_parse_fun_t) _parse_service,
                 service, &pmsg);

    if (pmsg) {
        w_obj_unref (service);
        if (msg)
            *msg = pmsg;
        else
            w_free (pmsg);
        service = NULL;
    }
    else {
        if (!service->name)
            service->name = w_str_dup (name);
    }

_return:
    if (path != name)
        w_free (path);
    return service;
}

