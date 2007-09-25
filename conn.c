/* conn.c - network connection state */

#include <stdlib.h>
#include <stdio.h>

#include "conn.h"
#include "net.h"
#include "util.h"
#include "prot.h"

/* Singly-linked list of free connections (the prev pointer isn't used here). */
static conn pool_front = NULL, pool_rear = NULL;

static int
pool_conn_p()
{
    return !!pool_front;
}

static conn
conn_alloc()
{
    conn c;

    if (!pool_conn_p()) return malloc(sizeof(struct conn));

    /* remove it from the list */
    c = pool_front;
    pool_front = c->next;
    c->prev = c->next = NULL;

    return c;
}

static void
conn_free(conn c)
{
    c->fd = 0;
    c->prev = c->next = NULL;
    if (pool_conn_p()) {
        pool_rear->next = c;
    } else {
        pool_front = c;
    }
    pool_rear = c;
}

conn
make_conn(int fd, char start_state)
{
    conn c;

    c = conn_alloc();
    if (!c) return warn("OOM"), NULL;

    c->fd = fd;
    c->state = start_state;
    c->cmd_read = 0;
    c->in_job = NULL;
    c->reserved_job = NULL;

    return c;
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
    if (c->in_job) free(c->in_job);
    c->in_job = c->reserved_job = NULL;

    conn_remove(c);
    conn_free(c);
}
