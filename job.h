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

#include <time.h>

typedef struct job *job;
typedef int(*job_cmp_fn)(job, job);

#include "tube.h"

#define JOB_STATE_INVALID 0
#define JOB_STATE_READY 1
#define JOB_STATE_RESERVED 2
#define JOB_STATE_BURIED 3
#define JOB_STATE_DELAYED 4

struct job {
    job prev, next; /* linked list of jobs */
    unsigned long long int id;
    unsigned int pri;
    unsigned int delay;
    unsigned int ttr;
    int body_size;
    time_t creation;
    time_t deadline;
    unsigned int timeout_ct;
    unsigned int release_ct;
    unsigned int bury_ct;
    unsigned int kick_ct;
    tube tube;
    void *reserver;
    void *binlog;
    char state;
    job ht_next; /* Next job in a hash table list */
    char body[];
};

#define make_job(pri,delay,ttr,body_size,tube) make_job_with_id(pri,delay,ttr,body_size,tube,0)

job allocate_job(int body_size);
job make_job_with_id(unsigned int pri, unsigned int delay, unsigned int ttr,
             int body_size, tube tube, unsigned long long id);
void job_free(job j);

/* Lookup a job by job ID */
job job_find(unsigned long long int job_id);

int job_pri_cmp(job a, job b);
int job_delay_cmp(job a, job b);

job job_copy(job j);

const char * job_state(job j);

int job_list_any_p(job head);
job job_remove(job j);
void job_insert(job head, job j);

unsigned long long int total_jobs();

void job_init();

#endif /*job_h*/
