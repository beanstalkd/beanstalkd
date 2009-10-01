/* job.h - a job in the queue */

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

#ifndef job_h
#define job_h

#include "config.h"

#if HAVE_STDINT_H
# include <stdint.h>
#endif /* else we get int types from config.h */

#include "util.h"

typedef struct job *job;
typedef int(*job_cmp_fn)(job, job);

#include "tube.h"

#define JOB_STATE_INVALID 0
#define JOB_STATE_READY 1
#define JOB_STATE_RESERVED 2
#define JOB_STATE_BURIED 3
#define JOB_STATE_DELAYED 4
#define JOB_STATE_COPY 5

/* If you modify this struct, you MUST increment binlog format version in
 * binlog.c. */
struct job {

    /* persistent fields; these get written to the binlog */
    uint64_t id;
    uint32_t pri;
    usec delay;
    usec ttr;
    int32_t body_size;
    usec created_at;
    usec deadline_at;
    uint32_t reserve_ct;
    uint32_t timeout_ct;
    uint32_t release_ct;
    uint32_t bury_ct;
    uint32_t kick_ct;
    uint8_t state;

    /* bookeeping fields; these are in-memory only */
    char pad[6];
    tube tube;
    job prev, next; /* linked list of jobs */
    job ht_next; /* Next job in a hash table list */
    size_t heap_index; /* where is this job in its current heap */
    void *binlog;
    void *reserver;
    size_t reserved_binlog_space;

    /* variable-size job data; written separately to the binlog */
    char body[];
};

#define make_job(pri,delay,ttr,body_size,tube) make_job_with_id(pri,delay,ttr,body_size,tube,0)

job allocate_job(int body_size);
job make_job_with_id(unsigned int pri, usec delay, usec ttr,
             int body_size, tube tube, uint64_t id);
void job_free(job j);

/* Lookup a job by job ID */
job job_find(uint64_t job_id);

int job_pri_cmp(job a, job b);
int job_delay_cmp(job a, job b);

job job_copy(job j);

const char * job_state(job j);

int job_list_any_p(job head);
job job_remove(job j);
void job_insert(job head, job j);

uint64_t total_jobs();

/* for unit tests */
size_t get_all_jobs_used();

void job_init();

#endif /*job_h*/
