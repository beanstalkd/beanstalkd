#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "dat.h"

const char *progname;

void
v()
{
}

static void
vwarnx(const char *err, const char *fmt, va_list args)
{
    fprintf(stderr, "%s: ", progname);
    if (fmt) {
        vfprintf(stderr, fmt, args);
        if (err) fprintf(stderr, ": %s", err);
    }
    fputc('\n', stderr);
}

void
warn(const char *fmt, ...)
{
    char *err = strerror(errno); /* must be done first thing */
    va_list args;

    va_start(args, fmt);
    vwarnx(err, fmt, args);
    va_end(args);
}

void
warnx(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vwarnx(NULL, fmt, args);
    va_end(args);
}


char*
fmtalloc(char *fmt, ...)
{
    int n;
    char *buf;
    va_list ap;

    // find out how much space is needed
    va_start(ap, fmt);
    n = vsnprintf(0, 0, fmt, ap) + 1; // include space for trailing NUL
    va_end(ap);

    buf = malloc(n);
    if (buf) {
        va_start(ap, fmt);
        vsnprintf(buf, n, fmt, ap);
        va_end(ap);
    }
    return buf;
}


// Zalloc allocates n bytes of zeroed memory and
// returns a pointer to it.
// If insufficient memory is available, zalloc returns 0.
void*
zalloc(int n)
{
    void *p;

    p = malloc(n);
    if (p) {
        memset(p, 0, n);
    }
    return p;
}
