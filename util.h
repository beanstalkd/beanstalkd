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
#define dprintf(fmt, args...) ((void) fprintf(stderr, fmt, ##args))
#else
#define dprintf(fmt, ...) ((void) 0)
#endif

#endif /*util_h*/
