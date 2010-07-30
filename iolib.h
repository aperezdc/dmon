/* Adam's IO library. */

#ifndef _IOLIB_H
#define _IOLIB_H 1

#ifndef NULL
#define NULL 0
#endif

#include <stdarg.h>

/* Default file descriptors. */
extern int fd_in, fd_out, fd_err;

/* A variable-length buffer for arbitrary data (i.e. nulls within it
   don't matter unless you're using bstr(). To declare a buffer, say
   something like "buffer b = BUFFER;". */
typedef struct {
	/* The data. */
	char *s;
	/* The length of the data. */
	int len;
	/* The size of the area that's actually allocated. */
	int size;
} buffer;
#define BUFFER {NULL, 0, 0}

/* Adjust the length of a buffer, keeping the existing contents. */
void bsetlength(buffer *b, int len);
/* Destroy a buffer. */
void bfree(buffer *b);
/* Set a buffer to a C string. */
void bmake(buffer *b, const char *s);
/* Append a block of memory to a buffer. */
void bappendm(buffer *dest, const char *src, int len);
/* Append a C string to a buffer. */
void bappends(buffer *dest, const char *s);
/* Append a byte to a buffer. */
void bappendc(buffer *dest, char c);
/* Append a buffer to another buffer. */
void bappend(buffer *dest, const buffer *src);
/* Get the length of a buffer. */
int blength(const buffer *b);
/* Get a buffer as a C string. */
const char *bstr(const buffer *b);
/* Find the first occurrence of byte c in the buffer.
   Returns the position, or -1 if not found. */
int bindex(const buffer *b, char c);
/* Replace character from with character to throughout buffer. */
void breplacec(buffer *b, char from, char to);
/* Remove n bytes from the start of b; if saveto != NULL, append the
   removed bytes to saveto. */
void bpopl(buffer *b, int n, buffer *saveto);

/* Write all n bytes from s to fd, ignoring EINTR. Return 0 on success, -1 on
   error. */
int writea(int fd, const char *s, int n);
/* Write some of a buffer to an fd.
   Returns number of bytes written on success, -1 on error. */
int writeb(int fd, const buffer *b);
/* Write all of a buffer to an fd, ignoring EINTR.
   Returns 0 on success, -1 on error. */
int writeba(int fd, const buffer *b);
/* Read at most n bytes from an fd into b.
   Returns number of bytes read on success, 0 on EOF, -1 on error. */
int readb(int fd, buffer *b, int n);
/* Read from an fd into b until EOF.
   Returns 0 on success, -1 on error. */
int readba(int fd, buffer *b);
/* Read at most max bytes from overflow, then fd into b until term is hit. Put
   any extra characters read after term into overflow.
   Return 1 on success, 0 on EOF, -1 on error. */
int readuntilb(int fd, buffer *b, int max, char term, buffer *overflow);
/* Read at most max bytes from overflow, then fd into b until \n is hit. Put
   any extra characters read after \n into overflow.
   Return 1 on success, 0 on EOF, -1 on error. */
int readlineb(int fd, buffer *b, int max, buffer *overflow);

/* Format the optional arguments onto fd. Usage is something like:
   format(some_fd, format, ...);
   format string contains:
     '@b' for buf *
     '@c' for char *
     '@i' for int (decimal)
     '@I' for unsigned int (decimal)
     '@x' for int (hex)
     '@X' for unsigned int (hex)
     '@l' for long (decimal)
     '@L' for unsigned long (decimal)
     '@a' for an IP address in network byte order as an unsigned long
   Returns 0 on success, -1 on error. */
int format(int fd, const char *format, ...);

/* Like format, but passing a list of arguments. */
int vformat(int fd, const char *format, va_list va);

/* Like format, but appends formatted data to a buffer. */
void bformat(buffer *b, const char *format, ...);

#endif

