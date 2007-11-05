/* job.c - a job in the queue */

#include <stdlib.h>
#include <string.h>

#include "job.h"
#include "util.h"

static unsigned long long int next_id = 1;

job
allocate_job(int body_size)
{
    job j;

    j = malloc(sizeof(struct job) + body_size);
    if (!j) return twarnx("OOM"), NULL;

    j->state = JOB_STATE_INVALID;
    j->creation = time(NULL);
    j->timeout_ct = j->release_ct = j->bury_ct = j->kick_ct = 0;
    j->body_size = body_size;
    j->next = j->prev = j; /* not in a linked list */

    return j;
}

job
make_job(unsigned int pri, int body_size)
{
    job j;

    j = allocate_job(body_size);
    if (!j) return twarnx("OOM"), NULL;

    j->id = next_id++;
    j->pri = pri;

    return j;
}

job
job_copy(job j)
{
    job n;

    if (!j) return NULL;

    n = malloc(sizeof(struct job) + j->body_size);
    if (!n) return twarnx("OOM"), NULL;

    n->id = j->id;
    n->pri = j->pri;
    n->body_size = j->body_size;
    memcpy(n->body, j->body, j->body_size);

    return n;
}

int
job_cmp(job a, job b)
{
    if (a->pri == b->pri) {
        /* we can't just subtract because id has too many bits */
        if (a->id > b->id) return 1;
        if (a->id < b->id) return -1;
        return 0;
    }
    return a->pri - b->pri;
}

const char *
job_state(job j)
{
    if (j->state == JOB_STATE_READY) return "ready";
    if (j->state == JOB_STATE_RESERVED) return "reserved";
    if (j->state == JOB_STATE_BURIED) return "buried";
    return "invalid";
}

int
job_list_any_p(job head)
{
    return head->next != head || head->prev != head;
}

job
job_remove(job j)
{
    if (!j) return NULL;
    if (!job_list_any_p(j)) return NULL; /* not in a doubly-linked list */

    j->next->prev = j->prev;
    j->prev->next = j->next;

    j->prev = j->next = j;

    return j;
}

void
job_insert(job head, job j)
{
    if (job_list_any_p(j)) return; /* already in a linked list */

    j->prev = head->prev;
    j->next = head;
    head->prev->next = j;
    head->prev = j;
}

