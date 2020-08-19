/*
 * conf.c
 * Copyright (C) 2020 Adrian Perez de Castro <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "deps/dbuf/dbuf.h"
#include "deps/cflag/cflag.h"
#include "deps/clog/clog.h"
#include "conf.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct parser {
    FILE        *input;
    int          look;
    unsigned     line, col;
    struct dbuf *errmsg;
};

enum {
    COMMENTCHAR = '#',
};

static bool
err(struct parser *p, const char *fmt, ...)
{
    dbuf_addfmt(p->errmsg, "%u:%u ", p->line, p->col);

    va_list args;
    va_start(args, fmt);
    dbuf_addfmtv(p->errmsg, fmt, args);
    va_end(args);

    return false;
}

static inline int
isnotcr(int c)
{
    return c != '\n';
}

static inline int
isnotspace(int c)
{
    return !isspace(c);
}

static int
nextch(struct parser *p)
{
    int c = fgetc(p->input);
    if (c == '\n') {
        p->col = 0;
        p->line++;
    }
    p->col++;
    return c;
}

static void
skipwhile(struct parser *p, int (*cond)(int))
{
    while (p->look != EOF && (*cond)(p->look))
        p->look = nextch(p);
}

static inline void
skipws(struct parser *p)
{
    skipwhile(p, isspace);
}

static void
advance(struct parser *p)
{
    do {
        if ((p->look = nextch(p)) == COMMENTCHAR)
            skipwhile(p, isnotcr);
    } while (p->look != EOF && p->look == COMMENTCHAR);
}

static void
takewhile(struct parser *p, int (*cond)(int), struct dbuf *result)
{
    dbuf_clear(result);
    while (p->look != EOF && (*cond)(p->look)) {
        dbuf_addch(result, p->look);
        advance(p);
    }
}

static bool
parse_word(struct parser *p, struct dbuf *result)
{
    takewhile(p, isnotspace, result);
    advance(p);
    skipws(p);
    return dbuf_size(result) > 0;
}

static bool
parse_string(struct parser *p, struct dbuf *result)
{
    dbuf_clear(result);

    int c = nextch(p);
    for (; c != '"' && c != EOF; c = nextch(p)) {
        if (c == '\\') {
            /* escaped sequences */
            switch ((c = nextch(p))) {
                case 'n': c = '\n'; break; /* carriage return */
                case 'r': c = '\r'; break; /* line feed */
                case 'b': c = '\b'; break; /* backspace */
                case 'e': c = 0x1b; break; /* escape */
                case 'a': c = '\a'; break; /* bell */
                case 't': c = '\t'; break; /* tab */
                case 'v': c = '\v'; break; /* vertical tab */
                case 'X': /* hex number */
                case 'x': { char num[3];
                            num[0] = nextch(p);
                            num[1] = nextch(p);
                            num[2] = '\0';
                            if (!isxdigit(num[0]) || !isxdigit(num[1]))
                                return err(p, "Invalid hex sequence");
                            c = strtol(num, NULL, 16);
                          } break;
            }
        }

        dbuf_addch(result, c);
    }

    /* Premature end of string. */
    if (c == EOF)
        return err(p, "Unterminated string");

    advance(p);
    skipws(p);

    return true;
}

static const struct cflag*
find_flag(const struct cflag *specs, const char *name)
{
    for (size_t i = 0; specs[i].name; i++)
        if (strcmp(specs[i].name, name) == 0)
            return &specs[i];
    return NULL;
}

static bool
parse_input(struct parser *p, const struct cflag *specs)
{
    struct dbuf option = DBUF_INIT;
    struct dbuf argument = DBUF_INIT;

    bool ok = true;

    while (p->look != EOF) {
        if (!parse_word(p, &option)) {
            ok = err(p, "Identifier expected");
            break;
        }

        const struct cflag *spec = find_flag(specs, dbuf_str(&option));
        if (!spec) {
            ok = err(p, "No such option %s", dbuf_str(&option));
            break;
        }

        enum cflag_status status;
        if ((*spec->func)(NULL, NULL) == CFLAG_NEEDS_ARG) {
            if (p->look == '"')
                ok = parse_string(p, &argument);
            else
                ok = parse_word(p, &argument);
            if (!ok) {
                err(p, "Expected argument for option %s", spec->name);
                break;
            }

            /* FIXME: Can we avoid this leak? */
            char *arg = strdup(dbuf_str(&argument));
            status = (*spec->func)(spec, arg);
            clog_debug("Config: %s \"%s\"", spec->name, arg);
        } else {
            status = (*spec->func)(spec, NULL);
            clog_debug("Config: %s", spec->name);
        }

        if (status != CFLAG_OK) {
            ok = err(p, "Argument '%s' for option %s is invalid",
                     dbuf_str(&argument), spec->name);
            break;
        }
    }

    dbuf_clear(&option);
    dbuf_clear(&argument);

    return ok;
}

bool
conf_parse(FILE               *input,
           const struct cflag *specs,
           struct dbuf        *err)
{
    struct parser p = {
        .errmsg = err,
        .input = input,
        .line = 1,
    };

    dbuf_clear(p.errmsg);

    advance(&p);
    skipws(&p);

    return parse_input(&p, specs);
}
