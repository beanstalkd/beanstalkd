/* util.c - util functions */

/* Copyright (C) 2007 Keith Rarick and Philotic Inc.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

char *progname; /* defined as extern in util.h */

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
        if (err) fprintf(stderr, ": %s", strerror(errno));
    }
    fputc('\n', stderr);
}

void
warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vwarnx(strerror(errno), fmt, args);
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
