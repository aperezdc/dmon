/*
 * dbuf.c
 * Copyright (C) 2020 Adrian Perez de Castro <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "dbuf.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum {
    CHUNKSIZE = 512,  /* bytes */
};

/* Wraps malloc(), realloc(), and free(). */
void*
mrealloc(void *ptr, size_t size)
{
    if (size) {
        ptr = ptr ? realloc(ptr, size) : malloc(size);
    } else if (ptr) {
        free(ptr);
        ptr = NULL;
    }
    return ptr;
}

/* Reallocates a buffer and sets the .alloc member. */
static inline void
brealloc(struct dbuf *b, size_t size)
{
    if (size) {
        const size_t new_size = CHUNKSIZE * ((size / CHUNKSIZE) + 1) + 1;
        assert(new_size >= size);
        if (new_size != b->alloc)
            b->data = mrealloc(b->data, (b->alloc = new_size));
    } else if (b->data) {
        free(b->data);
        b->data = NULL;
        b->alloc = 0;
    } else {
        assert(b->alloc == 0);
    }
}

/* Calls brealloc() and sets the .size member. */
static inline void
bresize(struct dbuf *b, size_t size)
{
    brealloc(b, size);
    b->size = size;
}

struct dbuf*
dbuf_new(size_t prealloc)
{
    struct dbuf *b = mrealloc(NULL, sizeof(struct dbuf));
    *b = DBUF_INIT;
    bresize(b, prealloc);
    return b;
}

void
dbuf_free(struct dbuf *b)
{
    assert(b);
    dbuf_clear(b);
    mrealloc(b, 0);
}

void
dbuf_resize(struct dbuf *b, size_t size)
{
    assert(b);
    bresize(b, size);
}

void
dbuf_clear(struct dbuf *b)
{
    assert(b);
    bresize(b, 0);
}

void
dbuf_addmem(struct dbuf *b, const void *data, size_t size)
{
    assert(b);
    assert(data);

    const size_t bsize = b->size;
    bresize(b, bsize + size);
    memcpy(b->data + bsize, data, size);
}

void
dbuf_addstr(struct dbuf *b, const char *s)
{
    assert(b);
    assert(s);

    const size_t bsize = b->size;
    const size_t slen = strlen(s);
    bresize(b, bsize + slen);
    memcpy(b->data + bsize, s, slen);
}

void
dbuf_addfmtv(struct dbuf *b, const char *format, va_list args)
{
    assert(b);
    assert(format);

    va_list saved;
    va_copy(saved, args);

    const size_t available = (b->alloc - b->size);
    const size_t needed = available
        ? vsnprintf((char*) b->data + b->size, available - 1, format, args)
        : vsnprintf(NULL, 0, format, args);

    if (needed >= available) {
        brealloc(b, b->alloc + needed);
        vsnprintf((char*) b->data + b->size, b->alloc - 1, format, saved);
    }

    b->size += needed;
    va_end(saved);
}

char*
dbuf_str(struct dbuf *b)
{
    assert(b);

    if (!b->alloc)
        brealloc(b, 1);

    b->data[b->size] = '\0';
    return (char*) b->data;
}
