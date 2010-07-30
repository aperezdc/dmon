#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include "iolib.h"

/* Default file descriptors. */
int fd_in = 0;
int fd_out = 1;
int fd_err = 2;

/* The granularity with which to allocate buffer sizes;
   the allocated size of a buffer will always be a multiple of this. */
#define BUFFERUNIT 256

/* Die with a constant error message. */
#define die(s) do { write(2, s, sizeof(s) - 1); exit(20); } while (0)

/* Adjust the size of a buffer without destroying its contents. */
static void bsetsize(buffer *b, int len) {
	if (len == 0) {
		if (b->s != NULL)
			free(b->s);
		b->s = NULL;
		b->size = 0;
	} else {
		/* This always rounds up the allocated size so that
		   adding characters to a buf one at a time doesn't
		   cause a realloc for each character. */
		int size = BUFFERUNIT * ((len / BUFFERUNIT) + 1);
		if (size < len)
			size = len;
		if (size != b->size) {
			b->s = realloc(b->s, size + 1);
			if (b->s == NULL)
				die("out of memory");
			b->size = size;
		}
	}
}

void bsetlength(buffer *b, int len) {
	bsetsize(b, len);
	b->len = len;
}

void bfree(buffer *b) {
	bsetlength(b, 0);
}

void bmake(buffer *b, const char *s) {
	bsetlength(b, 0);
	bappendm(b, s, strlen(s));
}

void bappendm(buffer *dest, const char *src, int len) {
	int l = dest->len;
	bsetlength(dest, l + len);
	memcpy(dest->s + l, src, len);
}

void bappends(buffer *dest, const char *s) {
	bappendm(dest, s, strlen(s));
}

void bappendc(buffer *dest, char c) {
	bappendm(dest, &c, 1);
}

void bappend(buffer *dest, const buffer *src) {
	bappendm(dest, src->s, src->len);
}

int blength(const buffer *b) {
	return b->len;
}

const char *bstr(const buffer *b) {
	if (b->len == 0) return "";
	b->s[b->len] = '\0';
	return b->s;
}

int bindex(const buffer *b, char c) {
	char *pos;

	if (b->len == 0)
		return -1;

	pos = (char *) memchr(b->s, c, b->len);
	if (pos == NULL)
		return -1;
	else
		return pos - b->s;
}

void breplacec(buffer *b, char from, char to) {
	char *p = b->s, *end = p + b->len;
	for (; p < end; p++) {
		if (*p == from) *p = to;
	}
}

void bpopl(buffer *b, int n, buffer *saveto) {
	if (saveto != NULL)
		bappendm(saveto, b->s, n);
	/* FIXME: It would be more efficient to maintain a "start" pointer
	   and only memmove once we have more than a certain amount of space
	   wasted. */
	b->len -= n;
	memmove(b->s, b->s + n, b->len);
}

int writea(int fd, const char *s, int n) {
	while (n > 0) {
		int c;

		do {
			c = write(fd, s, n);
		} while (c < 0 && errno == EINTR);
		if (c < 0)
			return -1;

		s += c;
		n -= c;
	}
	return 0;
}

int writeb(int fd, const buffer *b) {
	return write(fd, b->s, b->len);
}

int writeba(int fd, const buffer *b) {
	return writea(fd, b->s, b->len);
}

int readb(int fd, buffer *b, int n) {
	int c;

	if (n == 0)
		return 0;

	if (b->size < (b->len + n))
		bsetsize(b, b->len + n);

	c = read(fd, b->s + b->len, n);
	if (c > 0)
		b->len += c;

	return c;
}

int readba(int fd, buffer *b) {
	int c;
	do {
		c = readb(fd, b, 4096);
	} while (c > 0);
	return c;
}

int readuntilb(int fd, buffer *b, int max, char term, buffer *overflow) {
	while (1) {
		int c;
		char *pos;

		pos = (char *) memchr(overflow->s, term, overflow->len);
		if (pos != NULL) {
			/* Remove the \n from overflow, but also trim it
			   from the string read. */
			bpopl(overflow, 1 + pos - overflow->s, b);
			bsetlength(b, b->len - 1);
			return 1;
		}

		/* If no maximum size is specified, then read as much as
		   possible. */
		c = readb(fd, overflow, (max) ? (max - overflow->len) : 4096);
		if (c <= 0)
			return c;
	}
}

int readlineb(int fd, buffer *b, int max, buffer *overflow) {
	return readuntilb(fd, b, max, '\n', overflow);
}

/* Return the representation of a digit. */
static char format_digit(int n) {
	if (n < 10) return '0' + n;
	if (n < 36) return 'A' + (n - 10);
	return '!';
}

/* Format an unsigned long into the buffer. */
static void format_ulong(buffer *b, unsigned long n, int base) {
	if (n >= base) format_ulong(b, n / base, base);
	bappendc(b, format_digit(n % base));
}

static void format_ullong(buffer *b, unsigned long long n, int base) {
	if (n >= base) format_ullong(b, n / base, base);
	bappendc(b, format_digit(n % base));
}

/* Format a signed long into the buffer. */
static void format_long(buffer *b, long n, int base) {
	if (n < 0) {
		bappendc(b, '-');
		format_ulong(b, -n, base);
	} else {
		format_ulong(b, n, base);
	}
}

/* Format a list of arguments into the buffer. */
static void vaformat(buffer *b, const char *fmt, va_list va) {
	while (1) {
		const char *p = strchr(fmt, '@');
		if (p == NULL)
			break;

		bappendm(b, fmt, p - fmt);
		++p;

		switch (*p) {
		case 'c': {
			char *s = va_arg(va, char *);
			bappends(b, s);
			break;
		}
		case 'b': {
			buffer *src = va_arg(va, buffer *);
			bappend(b, src);
			break;
		}
		case 'i': {
			int i = va_arg(va, int);
			format_long(b, i, 10);
			break;
		}
		case 'I': {
			unsigned int i = va_arg(va, unsigned int);
			format_ulong(b, i, 10);
			break;
		}
		case 'x': {
			int i = va_arg(va, int);
			format_long(b, i, 16);
			break;
		}
		case 'X': {
			unsigned int i = va_arg(va, unsigned int);
			format_ulong(b, i, 16);
			break;
		}
		case 'l': {
			long i = va_arg(va, long);
			format_long(b, i, 10);
			break;
		}
		case 'L': {
			unsigned long i = va_arg(va, unsigned long);
			format_ulong(b, i, 10);
			break;
		}
		case 'U': {
			unsigned long long i = va_arg(va, unsigned long long);
			format_ullong(b, i, 10);
			break;
		}
		case 'a': {
			unsigned long i = va_arg(va, unsigned long);
			format_long(b, i & 0xff, 10);
			bappendc(b, '.');
			format_long(b, (i >> 8) & 0xff, 10);
			bappendc(b, '.');
			format_long(b, (i >> 16) & 0xff, 10);
			bappendc(b, '.');
			format_long(b, (i >> 24) & 0xff, 10);
			break;
		}
		case '@': {
			bappendc(b, '@');
			break;
		}
		case '\0':
			die("unexpected end of format");
		default:
			die("unknown format character");
		}

		fmt = p + 1;
	}
	bappends(b, fmt);
}

int format(int fd, const char *fmt, ...) {
	int r;
	va_list va;
	buffer b = BUFFER;

	va_start(va, fmt);
	vaformat(&b, fmt, va);
	r = writeba(fd, &b);
	bfree(&b);
	va_end(va);

	return r;
}


int vformat(int fd, const char *fmt, va_list va)
{
    int r;
    buffer b = BUFFER;

    vaformat(&b, fmt, va);
    r = writeba(fd, &b);
    bfree(&b);

    return r;
}

void bformat(buffer *b, const char *fmt, ...) {
	va_list va;

	va_start(va, fmt);
	vaformat(b, fmt, va);
	va_end(va);
}

