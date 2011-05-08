/* conn.c - network connection state */

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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include "dat.h"

#define SAFETY_MARGIN (1000000000) /* 1 second */

/* Doubly-linked list of free connections. */
static struct conn pool = { &pool, &pool };

static int cur_conn_ct = 0, cur_worker_ct = 0, cur_producer_ct = 0;
static uint tot_conn_ct = 0;

static conn
conn_alloc()
{
    conn c;

    c = conn_remove(pool.next);
    if (!c) {
        c = malloc(sizeof(struct conn));
    }

    return memset(c, 0, sizeof *c);
}

static void
conn_free(conn c)
{
    c->sock.fd = 0;
    conn_insert(&pool, c);
}

static void
on_watch(ms a, tube t, size_t i)
{
    tube_iref(t);
    t->watching_ct++;
}

static void
on_ignore(ms a, tube t, size_t i)
{
    t->watching_ct--;
    tube_dref(t);
}

conn
make_conn(int fd, char start_state, tube use, tube watch)
{
    job j;
    conn c;

    c = conn_alloc();
    if (!c) return twarn("OOM"), (conn) 0;

    ms_init(&c->watch, (ms_event_fn) on_watch, (ms_event_fn) on_ignore);
    if (!ms_append(&c->watch, watch)) {
        conn_free(c);
        return twarn("OOM"), (conn) 0;
    }

    c->use = NULL; /* initialize */
    TUBE_ASSIGN(c->use, use);
    use->using_ct++;

    c->sock.fd = fd;
    c->state = start_state;
    c->type = 0;
    c->cmd_read = 0;
    c->pending_timeout = -1;
    c->soonest_job = NULL;
    c->in_job = c->out_job = NULL;
    c->in_job_read = c->out_job_sent = 0;
    c->prev = c->next = c; /* must be out of a linked list right now */
    j = &c->reserved_jobs;
    j->prev = j->next = j;

    /* stats */
    cur_conn_ct++;
    tot_conn_ct++;

    return c;
}

void
conn_set_producer(conn c)
{
    if (c->type & CONN_TYPE_PRODUCER) return;
    c->type |= CONN_TYPE_PRODUCER;
    cur_producer_ct++; /* stats */
}

void
conn_set_worker(conn c)
{
    if (c->type & CONN_TYPE_WORKER) return;
    c->type |= CONN_TYPE_WORKER;
    cur_worker_ct++; /* stats */
}

int
count_cur_conns()
{
    return cur_conn_ct;
}

uint
count_tot_conns()
{
    return tot_conn_ct;
}

int
count_cur_producers()
{
    return cur_producer_ct;
}

int
count_cur_workers()
{
    return cur_worker_ct;
}

static int
has_reserved_job(conn c)
{
    return job_list_any_p(&c->reserved_jobs);
}


static int64
conntickat(conn c)
{
    int margin = 0, should_timeout = 0;
    int64 t = INT64_MAX;

    if (conn_waiting(c)) {
        margin = SAFETY_MARGIN;
    }

    if (has_reserved_job(c)) {
        t = soonest_job(c)->r.deadline_at - nanoseconds() - margin;
        should_timeout = 1;
    }
    if (c->pending_timeout >= 0) {
        t = min(t, ((int64)c->pending_timeout) * 1000000000);
        should_timeout = 1;
    }

    if (should_timeout) {
        return nanoseconds() + t;
    }
    return 0;
}


void
connwant(conn c, int rw, conn list)
{
    c->rw = rw;
    conn_insert(list, c);
    connsched(c);
}


void
connsched(conn c)
{
    c->tickat = conntickat(c);
    srvschedconn(c->srv, c);
}


static int
conn_list_any_p(conn head)
{
    return head->next != head || head->prev != head;
}

conn
conn_remove(conn c)
{
    if (!conn_list_any_p(c)) return NULL; /* not in a doubly-linked list */

    c->next->prev = c->prev;
    c->prev->next = c->next;

    c->prev = c->next = c;
    return c;
}

void
conn_insert(conn head, conn c)
{
    if (conn_list_any_p(c)) return; /* already in a linked list */

    c->prev = head->prev;
    c->next = head;
    head->prev->next = c;
    head->prev = c;
}

/* return the reserved job with the earliest deadline,
 * or NULL if there's no reserved job */
job
soonest_job(conn c)
{
    job j = NULL;
    job soonest = c->soonest_job;

    if (soonest == NULL) {
        for (j = c->reserved_jobs.next; j != &c->reserved_jobs; j = j->next) {
            if (j->r.deadline_at <= (soonest ? : j)->r.deadline_at) soonest = j;
        }
    }
    c->soonest_job = soonest;
    return soonest;
}

int
has_reserved_this_job(conn c, job j)
{
    return j && j->r.state == Reserved && j->reserver == c;
}

/* return true if c has a reserved job with less than one second until its
 * deadline */
int
conn_has_close_deadline(conn c)
{
    int64 t = nanoseconds();
    job j = soonest_job(c);

    return j && t >= j->r.deadline_at - SAFETY_MARGIN;
}

int
conn_ready(conn c)
{
    size_t i;

    for (i = 0; i < c->watch.used; i++) {
        if (((tube) c->watch.items[i])->ready.len) return 1;
    }
    return 0;
}


int
connless(conn a, conn b)
{
    return a->tickat < b->tickat;
}


void
connrec(conn c, int i)
{
    c->tickpos = i;
}


void
conn_close(conn c)
{
    sockwant(&c->sock, 0);
    close(c->sock.fd);

    job_free(c->in_job);

    /* was this a peek or stats command? */
    if (c->out_job && !c->out_job->r.id) job_free(c->out_job);

    c->in_job = c->out_job = NULL;
    c->in_job_read = 0;

    if (c->type & CONN_TYPE_PRODUCER) cur_producer_ct--; /* stats */
    if (c->type & CONN_TYPE_WORKER) cur_worker_ct--; /* stats */

    cur_conn_ct--; /* stats */

    remove_waiting_conn(c);
    conn_remove(c);
    if (has_reserved_job(c)) enqueue_reserved_jobs(c);

    ms_clear(&c->watch);
    c->use->using_ct--;
    TUBE_ASSIGN(c->use, NULL);

    conn_free(c);
}
