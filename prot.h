/* prot.h - protocol implementation header */

#ifndef prot_h
#define prot_h

#include "job.h"
#include "conn.h"

/* space for 16 Mi jobs */
#define HEAP_SIZE 16 * 1024 * 1024

#define URGENT_THRESHOLD 1024

/* measured in seconds */
#define RESERVATION_TIMEOUT 120

#define MSG_RESERVED "RESERVED"

void prot_init();

void reply(conn c, char *line, int len, int state);
void reply_job(conn c, job j, const char *word);

conn remove_waiting_conn(conn c);
void enqueue_waiting_conn(conn c);

int enqueue_job(job j, unsigned int delay);
job delay_q_peek();
job delay_q_take();
void bury_job(job j);
unsigned int kick_jobs(unsigned int n);
void process_queue();

job peek_job(unsigned long long int id);
job peek_buried_job();
job remove_buried_job(unsigned long long int id);

unsigned int get_ready_job_ct();
unsigned int get_delayed_job_ct();
unsigned int get_buried_job_ct();
unsigned int get_urgent_job_ct();
int count_cur_waiting();

#endif /*prot_h*/
