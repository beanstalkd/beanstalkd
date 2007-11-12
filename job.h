/* job.h - a job in the queue */

#ifndef job_h
#define job_h

#include <time.h>

#define JOB_STATE_INVALID 0
#define JOB_STATE_READY 1
#define JOB_STATE_RESERVED 2
#define JOB_STATE_BURIED 3
#define JOB_STATE_DELAY 4

typedef struct job *job;

struct job {
    job prev, next; /* linked list of jobs */
    unsigned long long int id;
    unsigned int pri;
    unsigned int delay;
    int body_size;
    time_t creation;
    time_t deadline;
    unsigned int timeout_ct;
    unsigned int release_ct;
    unsigned int bury_ct;
    unsigned int kick_ct;
    char state;
    char body[];
};

job allocate_job(int body_size);
job make_job(unsigned int pri, unsigned int delay, int body_size);

typedef int(*job_cmp_fn)(job, job);
int job_pri_cmp(job a, job b);
int job_delay_cmp(job a, job b);

job job_copy(job j);

const char * job_state(job j);

int job_list_any_p(job head);
job job_remove(job j);
void job_insert(job head, job j);

unsigned long long int total_jobs();

#endif /*job_h*/
