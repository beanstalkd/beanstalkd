/* prot.c - protocol implementation */

#include <stdio.h>
#include <string.h>

#include "prot.h"
#include "pq.h"
#include "job.h"
#include "conn.h"
#include "util.h"
#include "reserve.h"

static pq ready_q;

/* Doubly-linked list of waiting connections. */
static struct conn wait_queue = { &wait_queue, &wait_queue, 0 };
static struct job graveyard = { &graveyard, &graveyard, 0 };
static unsigned int buried_ct = 0, urgent_ct = 0;

static int
waiting_conn_p()
{
    return conn_list_any_p(&wait_queue);
}

static int
buried_job_p()
{
    return job_list_any_p(&graveyard);
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

    r = snprintf(c->reply_buf, LINE_BUF_SIZE, "%s %llu %u %u\r\n",
                 word, j->id, j->pri, j->body_size - 2);

    /* can't happen */
    if (r >= LINE_BUF_SIZE) return warn("truncated reply"), conn_close(c);

    return reply(c, c->reply_buf, strlen(c->reply_buf), STATE_SENDJOB);
}

static conn
next_waiting_conn()
{
    return conn_remove(wait_queue.next);
}

void
process_queue()
{
    job j;

    while (waiting_conn_p()) {
        j = pq_take(ready_q);
        if (!j) return;
        if (j->pri < URGENT_THRESHOLD) urgent_ct--;
        reserve_job(next_waiting_conn(), j);
    }
}

int
enqueue_job(job j)
{
    int r;

    r = pq_give(ready_q, j);
    if (!r) return 0;
    j->state = JOB_STATE_READY;
    if (j->pri < URGENT_THRESHOLD) urgent_ct++;
    process_queue();
    return 1;
}

void
bury_job(job j)
{
    job_insert(&graveyard, j);
    buried_ct++;
    j->state = JOB_STATE_BURIED;
    j->bury_ct++;
}

/* return the number of jobs successfully kicked */
int
kick_job()
{
    int r;
    job j;

    if (!buried_job_p()) return 0;
    j = job_remove(graveyard.next);
    buried_ct--;
    j->kick_ct++;
    r = enqueue_job(j);
    if (!r) return bury_job(j), 0;
    return 1;
}

job
peek_buried_job()
{
    return buried_job_p() ? graveyard.next : NULL;
}

static job
find_buried_job(unsigned long long int id)
{
    job j;

    for (j = graveyard.next; j != &graveyard; j = j->next) {
        if (j->id == id) return j;
    }
    return NULL;
}

void
enqueue_waiting_conn(conn c)
{
    conn_insert(&wait_queue, conn_remove(c) ? : c);
}

static job
find_reserved_job_in_wait_queue(unsigned long long int id)
{
    job j;
    conn c;

    for (c = wait_queue.next; c != &wait_queue; c = c->next) {
        j = find_reserved_job_in_conn(c, id);
        if (j) return j;
    }
    return NULL;
}

job
peek_job(unsigned long long int id)
{
    return pq_find(ready_q, id) ? :
           find_reserved_job(id) ? :
           find_reserved_job_in_wait_queue(id) ? :
           find_buried_job(id);
}

unsigned int
get_ready_job_ct()
{
    return pq_used(ready_q);
}

unsigned int
get_buried_job_ct()
{
    return buried_ct;
}

unsigned int
get_urgent_job_ct()
{
    return urgent_ct;
}

void
prot_init()
{
    ready_q = make_pq(HEAP_SIZE);
}
