/*
 * dbuf.h
 * Copyright (C) 2020 Adrian Perez de Castro <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef DBUF_H
#define DBUF_H

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if !(defined(__GNUC__) && __GNUC__ >= 3) && !defined(__attribute__)
# define __attribute__(dummy)
#endif

struct dbuf {
    uint8_t *data;
    size_t   size;
    size_t   alloc;
};

#define DBUF_INIT  ((struct dbuf) { .data = NULL, .size = 0, .alloc = 0 })

struct dbuf* dbuf_new(size_t prealloc)
    __attribute__((warn_unused_result));

void dbuf_free(struct dbuf*)
    __attribute__((nonnull(1)));

void dbuf_resize(struct dbuf*, size_t)
    __attribute__((nonnull(1)));

void dbuf_clear(struct dbuf*)
    __attribute__((nonnull(1)));

void dbuf_addmem(struct dbuf*, const void*, size_t)
    __attribute__((nonnull(1, 2)));

void dbuf_addstr(struct dbuf*, const char*)
    __attribute__((nonnull(1, 2)));

void dbuf_addfmtv(struct dbuf*, const char*, va_list)
    __attribute__((nonnull(1, 2)));

char* dbuf_str(struct dbuf*)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));


static inline size_t dbuf_size(const struct dbuf*)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

size_t
dbuf_size(const struct dbuf *b)
{
    assert(b);
    return b->size;
}


static inline uint8_t* dbuf_data(struct dbuf*)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

uint8_t*
dbuf_data(struct dbuf *b)
{
    assert(b);
    return b->data;
}


static inline const uint8_t* dbuf_cdata(const struct dbuf*)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

const uint8_t* dbuf_cdata(const struct dbuf *b)
{
    assert(b);
    return b->data;
}


static inline bool dbuf_empty(const struct dbuf*)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

bool
dbuf_empty(const struct dbuf *b)
{
    assert(b);
    return b->size == 0;
}


static inline void dbuf_addch(struct dbuf*, char)
    __attribute__((nonnull(1)));

void
dbuf_addch(struct dbuf *b, char c)
{
    assert(b);
    dbuf_addmem(b, &c, 1);
}


static inline void dbuf_addbuf(struct dbuf*, const struct dbuf*)
    __attribute__((nonnull(1, 2)));

void
dbuf_addbuf(struct dbuf *b, const struct dbuf *o)
{
    assert(b);
    assert(o);

    dbuf_addmem(b, dbuf_cdata(o), dbuf_size(o));
}

static inline void dbuf_addfmt(struct dbuf*, const char*, ...)
    __attribute__((nonnull(1, 2)));

void
dbuf_addfmt(struct dbuf *b, const char *format, ...)
{
    assert(b);
    assert(format);

    va_list args;
    va_start(args, format);
    dbuf_addfmtv(b, format, args);
    va_end(args);
}

#endif /* !DBUF_H */
