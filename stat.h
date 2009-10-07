/* stat.h - stats struct */

/* Copyright (C) 2008 Keith Rarick and Philotic Inc.

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

#ifndef stat_h
#define stat_h

#include "config.h"

#if HAVE_STDINT_H
# include <stdint.h>
#endif /* else we get int types from config.h */

struct stats {
    unsigned int urgent_ct;
    unsigned int waiting_ct;
    unsigned int buried_ct;
    unsigned int reserved_ct;
    uint64_t total_jobs_ct;
};

#endif /*stat_h*/
