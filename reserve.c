/* reserve.c - job reservations */

#include "prot.h"
#include "reserve.h"

static unsigned int cur_reserved_ct = 0;

void
reserve_job(conn c, job j)
{
    j->deadline = time(NULL) + RESERVATION_TIMEOUT;
    cur_reserved_ct++; /* stats */
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
    cur_reserved_ct--;
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
    cur_reserved_ct--;
    c->reserved_job = NULL;
    return j;
}

void
remove_this_reserved_job(conn c, job j)
{
    if (c->reserved_job == j) {
        cur_reserved_ct--;
        c->reserved_job = NULL;
    }
}

unsigned int
count_reserved_jobs()
{
    return cur_reserved_ct;
}

