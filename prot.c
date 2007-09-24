/* prot.c - protocol implementation */

#include <stdio.h>
#include <string.h>

#include "prot.h"
#include "pq.h"
#include "job.h"
#include "conn.h"
#include "util.h"

static pq ready_q;

static conn waiting_conn_front, waiting_conn_rear;

static int
waiting_conn_p()
{
    return !!waiting_conn_front;
}

void
reply(conn c, char *line, int len, int state)
{
    int r;

    r = conn_update_evq(c, EV_WRITE | EV_PERSIST, NULL);
    if (r == -1) return warn("conn_update_evq() failed"), conn_close(c);

    c->reply = line;
    c->reply_len = len;
    c->reply_sent = 0;
    c->state = state;
}

void
reply_job(conn c, const char *word)
{
    int r;
    job j = c->reserved_job;

    r = snprintf(c->reply_buf, LINE_BUF_SIZE, "%s %lld %d\r\n",
                 word, j->id, j->pri);

    /* can't happen */
    if (r >= LINE_BUF_SIZE) return warn("truncated reply"), conn_close(c);

    return reply(c, c->reply_buf, strlen(c->reply_buf), STATE_SENDJOB);
}

static void
reserve_job(conn c, job j)
{
    c->reserved_job = j;

    fprintf(stderr, "found job %p\n", c->reserved_job);

    return reply_job(c, MSG_RESERVED);
}

static conn
next_waiting_conn()
{
    conn c;

    if (!waiting_conn_p()) return NULL;

    c = waiting_conn_front;
    waiting_conn_front = c->next_waiting;

    return c;
}

void
process_queue()
{
    job j;

    warn("processing queue");
    while (waiting_conn_p() && (j = pq_take(ready_q))) {
        reserve_job(next_waiting_conn(), j);
    }
}

int
enqueue_job(job j)
{
    int r;

    r = pq_give(ready_q, j);
    if (r) return process_queue(), 1;
    return 0;
}

void
enqueue_waiting_conn(conn c)
{
    c->next_waiting = NULL;
    if (waiting_conn_p()) {
        waiting_conn_rear->next_waiting = c;
    } else {
        waiting_conn_front = c;
    }
    waiting_conn_rear = c;
}

void
prot_init()
{
    ready_q = make_pq(HEAP_SIZE);
}
