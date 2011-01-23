/* util.h - util functions */

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

#ifndef util_h
#define util_h

#include "config.h"

#if HAVE_STDINT_H
# include <stdint.h>
#endif /* else we get int types from config.h */

#include <stdlib.h>
#include <sys/time.h>

#define min(a,b) ((a)<(b)?(a):(b))

void v();

void warn(const char *fmt, ...);
void warnx(const char *fmt, ...);

extern char *progname;

#define twarn(fmt, args...) warn("%s:%d in %s: " fmt, \
                                 __FILE__, __LINE__, __func__, ##args)
#define twarnx(fmt, args...) warnx("%s:%d in %s: " fmt, \
                                   __FILE__, __LINE__, __func__, ##args)

#ifdef DEBUG
#define dbgprintf(fmt, args...) ((void) fprintf(stderr, fmt, ##args))
#else
#define dbgprintf(fmt, ...) ((void) 0)
#endif

typedef uint64_t usec;
#define USEC (1)
#define MSEC (1000 * USEC)
#define SECOND (1000 * MSEC)

usec now_usec(void);
usec usec_from_timeval(struct timeval *tv);
void timeval_from_usec(struct timeval *tv, usec t);

#endif /*util_h*/
