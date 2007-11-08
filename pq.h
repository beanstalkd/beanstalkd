/* pq.h - priority queue header */

#ifndef q_h
#define q_h

#include "job.h"

typedef struct pq {
    unsigned int size;
    unsigned int used;
    job_cmp_fn cmp;
    job heap[];
} *pq;

/* make a fixed-size priority queue of the given size */
pq make_pq(unsigned int size, job_cmp_fn cmp);

/* return 1 if the job was inserted, else 0 */
int pq_give(pq q, job j);

/* return a job if the queue contains jobs, else NULL */
job pq_take(pq q);

/* return a job that matches the given id, else NULL */
/* This is O(n), so don't do it much. */
job pq_find(pq q, unsigned long long int id);

unsigned int pq_used(pq q);

#endif /*q_h*/
