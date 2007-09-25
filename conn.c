/* conn.c - network connection state */

#include <stdlib.h>
#include <stdio.h>

#include "conn.h"
#include "net.h"
#include "util.h"
#include "prot.h"

/* Doubly-linked list of free connections. */
static struct conn pool = { &pool, &pool, 0 };

int cur_conn_ct = 0, cur_worker_ct = 0, cur_producer_ct = 0;

static int
pool_conn_p()
{
    return conn_list_empty_p(&pool);
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
    conn c;

    c = conn_alloc();
    if (!c) return warn("OOM"), NULL;

    c->fd = fd;
    c->state = start_state;
    c->type = 0;
    c->cmd_read = 0;
    c->in_job = c->out_job = c->reserved_job = NULL;
    c->in_job_read = c->out_job_sent = 0;

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

    event_set(&c->evq, c->fd, events, handler, c);

    r = event_add(&c->evq, NULL);
    if (r == -1) return -1;

    return 0;
}

int
conn_update_evq(conn c, const int events)
{
    int r;

    if (!c) return -1;

    if (c->evq.ev_events == events) return 0;

    /* If it's been added, try to delete it first */
    if (c->evq.ev_base) {
        r = event_del(&c->evq);
        if (r == -1) return -1;
    }

    return conn_set_evq(c, events, c->evq.ev_callback);
}

int
conn_list_empty_p(conn head)
{
    return head->next != head;
}

void
conn_remove(conn c)
{
    if (!c->next || !c->prev) return; /* not in a doubly-linked list */

    c->next->prev = c->prev;
    c->prev->next = c->next;

    c->prev = c->next = NULL;
}

void
conn_insert(conn head, conn c)
{
    if (c->prev || c->next) return; /* already in a linked list */

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

    if (c->reserved_job) enqueue_job(c->reserved_job);
    free(c->in_job);
    if (c->out_job != c->reserved_job) free(c->out_job); /* peek command? */
    c->in_job = c->out_job = c->reserved_job = NULL;


    if (c->type & CONN_TYPE_PRODUCER) cur_producer_ct--; /* stats */
    if (c->type & CONN_TYPE_WORKER) cur_worker_ct--; /* stats */

    conn_remove(c);
    conn_free(c);
}
