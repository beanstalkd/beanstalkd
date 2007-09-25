/* job.c - a job in the queue */

#include <stdlib.h>
#include <string.h>

#include "job.h"
#include "util.h"

static unsigned long long int next_id = 0;

job
make_job(unsigned int pri, int data_size)
{
    job j;

    j = malloc(sizeof(struct job) + data_size);
    if (!j) return warn("OOM"), NULL;

    j->id = next_id++;
    j->pri = pri;
    j->data_size = data_size;

    return j;
}

job
job_copy(job j)
{
    job n;

    if (!j) return NULL;

    n = malloc(sizeof(struct job) + j->data_size);
    if (!n) return warn("OOM"), NULL;

    n->id = j->id;
    n->pri = j->pri;
    n->data_size = j->data_size;
    memcpy(n->data, j->data, j->data_size);

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
