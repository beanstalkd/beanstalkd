/* prot.h - protocol implementation header */

#ifndef prot_h
#define prot_h

#include "job.h"
#include "conn.h"

/* space for 16 Mi jobs */
#define HEAP_SIZE 16 * 1024 * 1024

/* measured in seconds */
#define RESERVATION_TIMEOUT 120

#define MSG_RESERVED "RESERVED"

void prot_init();

void reply(conn c, char *line, int len, int state);
void reply_job(conn c, job j, const char *word);

void enqueue_waiting_conn(conn c);

int enqueue_job(job j);
void process_queue();

job peek_job(unsigned long long int id);

int has_reserved_job(conn c);
job soonest_job(conn c);
void enqueue_reserved_jobs(conn c);
int has_reserved_this_job(conn c, job j);
job remove_reserved_job(conn c, unsigned long long int id);
void job_remove(conn c, job j);

int count_reserved_jobs();
unsigned int count_ready_jobs();

#endif /*prot_h*/
