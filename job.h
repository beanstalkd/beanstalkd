/* job.h - a job in the queue */

#ifndef job_h
#define job_h

typedef struct job {
    unsigned long long int id;
    unsigned int pri;
    int data_size;
    int data_read;
    unsigned char data[];
} *job;

job make_job(unsigned int pri, int data_size);

int job_cmp(job a, job b);

#endif /*job_h*/
