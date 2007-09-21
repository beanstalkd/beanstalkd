/* job.c - a job in the queue */

#include <stdlib.h>

#include "job.h"
#include "util.h"

job
make_job(unsigned int pri, int data_size)
{
    job j;

    j = malloc(sizeof(struct job) + data_size);
    if (!j) return warn("OOM"), NULL;

    j->pri = pri;
    j->data_size = data_size;
    j->data_read = 0;

    return j;
}
