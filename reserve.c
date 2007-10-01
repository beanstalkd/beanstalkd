/* reserve.c - job reservations */

#include "job.h"
#include "prot.h"
#include "reserve.h"

static unsigned int cur_reserved_ct = 0;

void
reserve_job(conn c, job j)
{
    j->deadline = time(NULL) + RESERVATION_TIMEOUT;
    cur_reserved_ct++; /* stats */
    job_insert(&c->reserved_jobs, j);
    return reply_job(c, j, MSG_RESERVED);
}

int
has_reserved_job(conn c)
{
    return job_list_any_p(&c->reserved_jobs);
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

void
enqueue_reserved_jobs(conn c)
{
    while (job_list_any_p(&c->reserved_jobs)) {
        enqueue_job(job_remove(c->reserved_jobs.next));
        cur_reserved_ct--;
    }
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

static job
find_reserved_job(conn c, unsigned long long int id)
{
    job j;

    for (j = c->reserved_jobs.next; j != &c->reserved_jobs; j = j->next) {
        if (j->id == id) return j;
    }
    return NULL;
}

job
remove_reserved_job(conn c, unsigned long long int id)
{
    return remove_this_reserved_job(c, find_reserved_job(c, id));
}

/* j can be NULL */
job
remove_this_reserved_job(conn c, job j)
{
    j = job_remove(j);
    if (j) cur_reserved_ct--;
    return j;
}

unsigned int
count_reserved_jobs()
{
    return cur_reserved_ct;
}

