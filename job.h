/* job.h - a job in the queue */

#ifndef job_h
#define job_h

#include <time.h>

typedef struct job {
    unsigned long long int id;
    unsigned int pri;
    int body_size;
    time_t deadline;
    char body[];
} *job;

job allocate_job(int body_size);
job make_job(unsigned int pri, int body_size);

int job_cmp(job a, job b);

job job_copy(job j);

#endif /*job_h*/
