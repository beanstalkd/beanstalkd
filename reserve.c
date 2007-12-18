/* reserve.c - job reservations */

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

#include "job.h"
#include "prot.h"
#include "reserve.h"

static unsigned int cur_reserved_ct = 0;

/* Doubly-linked list of connections with at least one reserved job. */
static struct conn running = { &running, &running, 0 };

void
reserve_job(conn c, job j)
{
    j->deadline = time(NULL) + j->ttr;
    cur_reserved_ct++; /* stats */
    conn_insert(&running, c);
    j->state = JOB_STATE_RESERVED;
    job_insert(&c->reserved_jobs, j);
    return reply_job(c, j, MSG_RESERVED);
}

int
has_reserved_job(conn c)
{
    return job_list_any_p(&c->reserved_jobs);
}

/* return the reserved job with the earliest deadline,
 * or NULL if there's no reserved job */
job
soonest_job(conn c)
{
    job j, soonest = NULL;

    for (j = c->reserved_jobs.next; j != &c->reserved_jobs; j = j->next) {
        if (j->deadline <= (soonest ? : j)->deadline) soonest = j;
    }
    return soonest;
}

void
enqueue_reserved_jobs(conn c)
{
    int r;
    job j;

    while (job_list_any_p(&c->reserved_jobs)) {
        j = job_remove(c->reserved_jobs.next);
        r = enqueue_job(j, 0);
        if (!r) bury_job(j);
        cur_reserved_ct--;
        if (!job_list_any_p(&c->reserved_jobs)) conn_remove(c);
    }
}

int
has_reserved_this_job(conn c, job needle)
{
    job j;

    for (j = c->reserved_jobs.next; j != &c->reserved_jobs; j = j->next) {
        if (needle == j) return 1;
    }
    return 0;
}

static job
find_reserved_job_in_conn(conn c, unsigned long long int id)
{
    job j;

    for (j = c->reserved_jobs.next; j != &c->reserved_jobs; j = j->next) {
        if (j->id == id) return j;
    }
    return NULL;
}

job
remove_reserved_job(conn c, unsigned long long int id)
{
    return remove_this_reserved_job(c, find_reserved_job_in_conn(c, id));
}

/* j can be NULL */
job
remove_this_reserved_job(conn c, job j)
{
    j = job_remove(j);
    if (j) cur_reserved_ct--;
    if (!job_list_any_p(&c->reserved_jobs)) conn_remove(c);
    return j;
}

job
find_reserved_job_in_list(conn list, unsigned long long int id)
{
    job j;
    conn c;

    for (c = list->next; c != list; c = c->next) {
        j = find_reserved_job_in_conn(c, id);
        if (j) return j;
    }
    return NULL;
}

job
find_reserved_job(unsigned long long int id)
{
    return find_reserved_job_in_list(&running, id);
}

unsigned int
get_reserved_job_ct()
{
    return cur_reserved_ct;
}

