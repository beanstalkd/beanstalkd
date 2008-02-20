/* pq.h - priority queue header */

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

#ifndef q_h
#define q_h

typedef struct pq *pq;

#include "job.h"

struct pq {
    unsigned int cap;
    unsigned int used;
    job_cmp_fn cmp;
    job *heap;
};

/* initialize a priority queue */
void pq_init(pq q, job_cmp_fn cmp);

void pq_clear(pq q);

/* return 1 if the job was inserted, else 0 */
int pq_give(pq q, job j);

/* return a job if the queue contains jobs, else NULL */
job pq_take(pq q);

/* return a job if the queue contains jobs, else NULL */
job pq_peek(pq q);

/* return a job that matches the given id, else NULL */
/* This is O(n), so don't do it much. */
job pq_find(pq q, unsigned long long int id);

unsigned int pq_used(pq q);

#endif /*q_h*/
