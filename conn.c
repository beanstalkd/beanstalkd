/* conn.c - network connection state */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "conn.h"
#include "net.h"
#include "util.h"
#include "prot.h"
#include "reserve.h"

/* Doubly-linked list of free connections. */
static struct conn pool = { &pool, &pool, 0 };

int cur_conn_ct = 0, cur_worker_ct = 0, cur_producer_ct = 0;

static int
pool_conn_p()
{
    return conn_list_any_p(&pool);
}

static conn
conn_alloc()
{
    conn c;

    if (!pool_conn_p()) return malloc(sizeof(struct conn));

    /* remove it from the list */
    c = pool.next;
    conn_remove(c);

    return c;
}

static void
conn_free(conn c)
{
    c->fd = 0;
    conn_insert(&pool, c);
}

conn
make_conn(int fd, char start_state)
{
    job j;
    conn c;

    c = conn_alloc();
    if (!c) return warn("OOM"), NULL;

    c->fd = fd;
    c->state = start_state;
    c->type = 0;
    c->cmd_read = 0;
    c->in_job = c->out_job = NULL;
    c->in_job_read = c->out_job_sent = 0;
    c->prev = c->next = c; /* must be out of a linked list right now */
    j = &c->reserved_jobs;
    j->prev = j->next = j;

    cur_conn_ct++; /* stats */

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

int
conn_set_evq(conn c, const int events, evh handler)
{
    int r;
    struct timeval tv = {0, 0};

    event_set(&c->evq, c->fd, events, handler, c);

    if (has_reserved_job(c)) tv.tv_sec = soonest_job(c)->deadline - time(NULL);

    r = event_add(&c->evq, has_reserved_job(c) ? &tv : NULL);
    if (r == -1) return -1;

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

int
conn_list_any_p(conn head)
{
    return head->next != head || head->prev != head;
}

void
conn_remove(conn c)
{
    if (!conn_list_any_p(c)) return; /* not in a doubly-linked list */

    c->next->prev = c->prev;
    c->prev->next = c->next;

    c->prev = c->next = c;
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

void
conn_close(conn c)
{
    event_del(&c->evq);

    close(c->fd);

    free(c->in_job);

    /* was this a peek or stats command? */
    if (!has_reserved_this_job(c, c->out_job)) free(c->out_job);

    if (has_reserved_job(c)) enqueue_reserved_jobs(c);
    c->in_job = c->out_job = NULL;


    if (c->type & CONN_TYPE_PRODUCER) cur_producer_ct--; /* stats */
    if (c->type & CONN_TYPE_WORKER) cur_worker_ct--; /* stats */

    cur_conn_ct--; /* stats */

    conn_remove(c);
    conn_free(c);
}
