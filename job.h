/* job.h - a job in the queue */

#ifndef job_h
#define job_h

typedef struct job {
    unsigned int pri;
    int data_size;
    int data_read;
    unsigned char data[];
} *job;

job make_job(unsigned int pri, int data_size);

#endif /*job_h*/
