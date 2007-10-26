/* reserve.h - job reservations header */

#ifndef reserve_h
#define reserve_H

#include "job.h"
#include "conn.h"

void reserve_job(conn c, job j);
int has_reserved_job(conn c);
job soonest_job(conn c);
void enqueue_reserved_jobs(conn c);
int has_reserved_this_job(conn c, job j);
job remove_reserved_job(conn c, unsigned long long int id);
job remove_this_reserved_job(conn c, job j);
job find_reserved_job(unsigned long long int id);
job find_reserved_job_in_list(conn list, unsigned long long int id);

unsigned int get_reserved_job_ct();

#endif /*reserve_h*/
