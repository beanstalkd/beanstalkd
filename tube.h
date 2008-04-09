/* tube.h - tubes header */

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

#ifndef tube_h
#define tube_h

typedef struct tube *tube;

#include "stat.h"
#include "job.h"
#include "pq.h"
#include "ms.h"

#define MAX_TUBE_NAME_LEN 201

struct tube {
    unsigned int refs;
    char name[MAX_TUBE_NAME_LEN];
    struct pq ready;
    struct pq delay;
    struct job buried;
    struct ms waiting; /* set of conns */
    struct stats stat;
    unsigned int using_ct;
    unsigned int watching_ct;
};

tube make_tube(const char *name);
void tube_dref(tube t);
void tube_iref(tube t);
#define TUBE_ASSIGN(a,b) (tube_dref(a), (a) = (b), tube_iref(a))

#endif /*tube_h*/
