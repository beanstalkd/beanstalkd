/* reserve.c - job reservations */

#include "prot.h"
#include "reserve.h"

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

