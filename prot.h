/* prot.h - protocol implementation header */

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

#ifndef prot_h
#define prot_h

#include "job.h"
#include "conn.h"

#define CONSTSTRLEN(m) (sizeof(m) - 1)

/* space for 16 Mi jobs */
#define HEAP_SIZE 16 * 1024 * 1024

#define URGENT_THRESHOLD 1024

#define MSG_RESERVED "RESERVED"

void prot_init();

void reply_job(conn c, job j, const char *word);

conn remove_waiting_conn(conn c);

int enqueue_job(job j, unsigned int delay);
void bury_job(job j);

void enter_drain_mode(int sig);
void h_accept(const int fd, const short which, struct event *ev);

#endif /*prot_h*/
