/* prot.c - protocol implementation */

#include <stdio.h>
#include <string.h>

#include "prot.h"
#include "pq.h"
#include "job.h"
#include "conn.h"
#include "util.h"

static pq ready_q;

/* Doubly-linked list of waiting connections. */
static struct conn wait_queue = { &wait_queue, &wait_queue, 0 };

/* Doubly-linked list of connections running jobs. */
static struct conn running_list = { &running_list, &running_list, 0 };

static int
waiting_conn_p()
{
    return conn_list_any_p(&wait_queue);
}

void
reply(conn c, char *line, int len, int state)
{
    int r;

    r = conn_update_evq(c, EV_WRITE | EV_PERSIST);
    if (r == -1) return warn("conn_update_evq() failed"), conn_close(c);

    c->reply = line;
    c->reply_len = len;
    c->reply_sent = 0;
    c->state = state;
}

void
reply_job(conn c, job j, const char *word)
{
    int r;

    /* tell this connection which job to send */
    c->out_job = j;
    c->out_job_sent = 0;

    r = snprintf(c->reply_buf, LINE_BUF_SIZE, "%s %lld %d %d\r\n",
                 word, j->id, j->pri, j->body_size - 2);

    /* can't happen */
    if (r >= LINE_BUF_SIZE) return warn("truncated reply"), conn_close(c);

    return reply(c, c->reply_buf, strlen(c->reply_buf), STATE_SENDJOB);
}

static void
reserve_job(conn c, job j)
{
    j->deadline = time(NULL) + RESERVATION_TIMEOUT;
    if (!has_reserved_job(c)) conn_insert(&running_list, c);
    c->reserved_job = j;
    return reply_job(c, j, MSG_RESERVED);
}

int
has_reserved_job(conn c)
{
    return !!c->reserved_job;
}

/* return the reserved job with the earliest deadline,
 * or NULL if there's no reserved job */
job
soonest_job(conn c)
{
    return c->reserved_job;
}

void
enqueue_reserved_jobs(conn c)
{
    enqueue_job(c->reserved_job);
    c->reserved_job = NULL;
}

int
has_reserved_this_job(conn c, job j)
{
    return c->reserved_job == j;
}

job
remove_reserved_job(conn c, unsigned long long int id)
{
    job j;

    if (!c->reserved_job) return NULL;
    if (id != c->reserved_job->id) return NULL;
    j = c->reserved_job;
    c->reserved_job = NULL;
    return j;
}

void
job_remove(conn c, job j)
{
    if (c->reserved_job == j) c->reserved_job = NULL;
}

int
count_reserved_jobs()
{
    int count = 0;
    conn c = &running_list;

    for (c = c->next; c != &running_list; c = c->next) count++;
    return count;
}

static conn
next_waiting_conn()
{
    conn c;

    if (!waiting_conn_p()) return NULL;

    c = wait_queue.next;
    conn_remove(c);

    return c;
}

void
process_queue()
{
    job j;

    while (waiting_conn_p()) {
        j = pq_take(ready_q);
        if (!j) return;
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
    conn_insert(&wait_queue, c);
}

job
peek_job(unsigned long long int id)
{
    return pq_find(ready_q, id);
}

unsigned int
count_ready_jobs()
{
    return pq_used(ready_q);
}

void
prot_init()
{
    ready_q = make_pq(HEAP_SIZE);
}
