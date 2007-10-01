/* reserve.h - job reservations header */

#ifndef reserve_h
#define reserve_H

#include "job.h"
#include "conn.h"

int has_reserved_job(conn c);
job soonest_job(conn c);
void enqueue_reserved_jobs(conn c);
int has_reserved_this_job(conn c, job j);
job remove_reserved_job(conn c, unsigned long long int id);
void job_remove(conn c, job j);


#endif /*reserve_h*/
