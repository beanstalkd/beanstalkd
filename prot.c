/* prot.c - protocol implementation */

#include <stdio.h>
#include <string.h>

#include "prot.h"
#include "pq.h"
#include "job.h"
#include "conn.h"
#include "util.h"
#include "reserve.h"
#include "net.h"

static pq ready_q;
static pq delay_q;

/* Doubly-linked list of waiting connections. */
static struct conn wait_queue = { &wait_queue, &wait_queue, 0 };
static struct job graveyard = { &graveyard, &graveyard, 0 };
static unsigned int buried_ct = 0, urgent_ct = 0, waiting_ct = 0;

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
    if (r == -1) return twarnx("conn_update_evq() failed"), conn_close(c);

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
    if (r >= LINE_BUF_SIZE) return twarnx("truncated reply"), conn_close(c);

    return reply(c, c->reply_buf, strlen(c->reply_buf), STATE_SENDJOB);
}

conn
remove_waiting_conn(conn c)
{
    if (!(c->type & CONN_TYPE_WAITING)) return NULL;
    c->type &= ~CONN_TYPE_WAITING;
    waiting_ct--;
    return conn_remove(c);
}

void
process_queue()
{
    job j;

    while (waiting_conn_p()) {
        j = pq_take(ready_q);
        if (!j) return;
        if (j->pri < URGENT_THRESHOLD) urgent_ct--;
        reserve_job(remove_waiting_conn(wait_queue.next), j);
    }
}

int
enqueue_job(job j, unsigned int delay)
{
    int r;

    if (delay) {
        j->deadline = time(NULL) + delay;
        r = pq_give(delay_q, j);
        if (!r) return 0;
        j->state = JOB_STATE_DELAY;
        set_main_timeout(pq_peek(delay_q)->deadline);
    } else {
        r = pq_give(ready_q, j);
        if (!r) return 0;
        j->state = JOB_STATE_READY;
        if (j->pri < URGENT_THRESHOLD) urgent_ct++;
    }
    process_queue();
    return 1;
}

job
delay_q_peek()
{
    return pq_peek(delay_q);
}

job
delay_q_take()
{
    return pq_take(delay_q);
}

void
bury_job(job j)
{
    job_insert(&graveyard, j);
    buried_ct++;
    j->state = JOB_STATE_BURIED;
    j->bury_ct++;
}

static job
remove_this_buried_job(job j)
{
    j = job_remove(j);
    if (j) buried_ct--;
    return j;
}

static int
kick_buried_job()
{
    int r;
    job j;

    if (!buried_job_p()) return 0;
    j = remove_this_buried_job(graveyard.next);
    j->kick_ct++;
    r = enqueue_job(j, 0);
    if (r) return 1;

    /* ready_q is full, so bury it */
    bury_job(j);
    return 0;
}

static int
kick_delayed_job()
{
    int r;
    job j;

    if (get_delayed_job_ct() < 1) return 0;
    j = delay_q_take();
    j->kick_ct++;
    r = enqueue_job(j, 0);
    if (r) return 1;

    /* ready_q is full, so delay it again */
    r = enqueue_job(j, j->delay);
    if (r) return 0;

    /* last resort */
    bury_job(j);
    return 0;
}

/* return the number of jobs successfully kicked */
static unsigned int
kick_buried_jobs(unsigned int n)
{
    unsigned int i;
    for (i = 0; (i < n) && kick_buried_job(); ++i);
    return i;
}

/* return the number of jobs successfully kicked */
static unsigned int
kick_delayed_jobs(unsigned int n)
{
    unsigned int i;
    for (i = 0; (i < n) && kick_delayed_job(); ++i);
    return i;
}

unsigned int
kick_jobs(unsigned int n)
{
    if (buried_job_p()) return kick_buried_jobs(n);
    return kick_delayed_jobs(n);
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

job
remove_buried_job(unsigned long long int id)
{
    return remove_this_buried_job(find_buried_job(id));
}

void
enqueue_waiting_conn(conn c)
{
    waiting_ct++;
    c->type |= CONN_TYPE_WAITING;
    conn_insert(&wait_queue, conn_remove(c) ? : c);
}

job
peek_job(unsigned long long int id)
{
    return pq_find(ready_q, id) ? :
           pq_find(delay_q, id) ? :
           find_reserved_job(id) ? :
           find_reserved_job_in_list(&wait_queue, id) ? :
           find_buried_job(id);
}

unsigned int
get_ready_job_ct()
{
    return pq_used(ready_q);
}

unsigned int
get_delayed_job_ct()
{
    return pq_used(delay_q);
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

int
count_cur_waiting()
{
    return waiting_ct;
}

void
prot_init()
{
    ready_q = make_pq(HEAP_SIZE, job_pri_cmp);
    delay_q = make_pq(HEAP_SIZE, job_delay_cmp);
}
