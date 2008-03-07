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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "conn.h"
#include "net.h"
#include "util.h"
#include "prot.h"

#define SAFETY_MARGIN 1 /* seconds */

/* Doubly-linked list of free connections. */
static struct conn pool = { &pool, &pool, 0 };

static int cur_conn_ct = 0, cur_worker_ct = 0, cur_producer_ct = 0;
static unsigned int tot_conn_ct = 0;

static conn
conn_alloc()
{
    return conn_remove(pool.next) ? : malloc(sizeof(struct conn));
}

static void
conn_free(conn c)
{
    c->fd = 0;
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
    if (!c) return twarn("OOM"), NULL;

    ms_init(&c->watch, (ms_event_fn) on_watch, (ms_event_fn) on_ignore);
    if (!ms_append(&c->watch, watch)) {
        conn_free(c);
        return twarn("OOM"), NULL;
    }

    c->use = NULL; /* initialize */
    TUBE_ASSIGN(c->use, use);
    use->using_ct++;

    c->fd = fd;
    c->state = start_state;
    c->type = 0;
    c->cmd_read = 0;
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

unsigned int
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

int
conn_set_evq(conn c, const int events, evh handler)
{
    int r;
    struct timeval tv = {0, 0};

    event_set(&c->evq, c->fd, events, handler, c);

    if (has_reserved_job(c)) tv.tv_sec = soonest_job(c)->deadline - time(NULL);

    r = event_add(&c->evq, has_reserved_job(c) ? &tv : NULL);
    if (r == -1) return twarn("event_add() err %d", errno), -1;

    return 0;
}

int
conn_update_evq(conn c, const int events)
{
    int r;

    if (!c) return -1;

    /* If it's been added, try to delete it first */
    if (c->evq.ev_base) {
        r = event_del(&c->evq);
        if (r == -1) return -1;
    }

    return conn_set_evq(c, events, c->evq.ev_callback);
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
    job j, soonest = NULL;

    for (j = c->reserved_jobs.next; j != &c->reserved_jobs; j = j->next) {
        if (j->deadline <= (soonest ? : j)->deadline) soonest = j;
    }
    return soonest;
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

/* return true if c has a reserved job with less than one second until its
 * deadline */
int
conn_has_close_deadline(conn c)
{
    time_t t = time(NULL);
    job j = soonest_job(c);

    return j && t >= j->deadline - SAFETY_MARGIN;
}

void
conn_close(conn c)
{
    event_del(&c->evq);

    close(c->fd);

    job_free(c->in_job);

    /* was this a peek or stats command? */
    if (!has_reserved_this_job(c, c->out_job)) job_free(c->out_job);

    c->in_job = c->out_job = NULL;

    if (c->type & CONN_TYPE_PRODUCER) cur_producer_ct--; /* stats */
    if (c->type & CONN_TYPE_WORKER) cur_worker_ct--; /* stats */

    cur_conn_ct--; /* stats */

    unbrake(NULL);
    remove_waiting_conn(c);
    conn_remove(c);
    if (has_reserved_job(c)) enqueue_reserved_jobs(c);

    ms_clear(&c->watch);
    c->use->using_ct--;
    TUBE_ASSIGN(c->use, NULL);

    conn_free(c);
}
