/* pq.h - priority queue header */

#ifndef q_h
#define q_h

#include "job.h"

/* space for 16 Mi jobs */
#define HEAP_SIZE 16 * 1024 * 1024

typedef struct pq {
    unsigned int size;
    unsigned int used;
    job heap[];
} *pq;

/* make a fixed-size priority queue of the given size */
pq make_pq(unsigned int size);

/* return 1 if the job was inserted, else 0 */
int pq_give(pq q, job j);

/* return a job if the queue contains jobs, else NULL */
job pq_take(pq q);

#endif /*q_h*/
