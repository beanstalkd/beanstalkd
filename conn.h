/* conn.h - network connection state */

#ifndef conn_h
#define conn_h

#include "event.h"
#include "job.h"

#define STATE_WANTCOMMAND 0
#define STATE_WANTDATA 1
#define STATE_SENDJOB 2
#define STATE_SENDWORD 3
#define STATE_WAIT 4

/* A command can be at most LINE_BUF_SIZE chars, including "\r\n". This value
 * MUST be enough to hold the longest possible command or reply line, which is
 * currently "release 18446744073709551615 4294967295 4294967295\r\n". */
#define LINE_BUF_SIZE 54

#define OP_UNKNOWN 0
#define OP_PUT 1
#define OP_PEEKJOB 2
#define OP_RESERVE 3
#define OP_DELETE 4
#define OP_RELEASE 5
#define OP_BURY 6
#define OP_KICK 7
#define OP_STATS 8
#define OP_JOBSTATS 9
#define OP_PEEK 10

/* CONN_TYPE_* are bit masks */
#define CONN_TYPE_PRODUCER 1
#define CONN_TYPE_WORKER 2
#define CONN_TYPE_WAITING 4

typedef struct conn *conn;

struct conn {
    conn prev, next; /* linked list of connections */
    int fd;
    char state;
    char type;
    struct event evq;

    /* we cannot share this buffer with the reply line because we might read in
     * command line data for a subsequent command, and we need to store it
     * here. */
    char cmd[LINE_BUF_SIZE]; /* this string is NOT NUL-terminated */
    int cmd_len;
    int cmd_read;
    char *reply;
    int reply_len;
    int reply_sent;
    char reply_buf[LINE_BUF_SIZE]; /* this string IS NUL-terminated */
    job in_job;
    int in_job_read;
    job out_job;
    int out_job_sent;
    struct job reserved_jobs; /* doubly-linked list header */
};

void conn_init();

conn make_conn(int fd, char start_state);

int conn_set_evq(conn c, const int events, evh handler);
int conn_update_evq(conn c, const int flags);

void conn_close(conn c);

int conn_list_any_p(conn head);
conn conn_remove(conn c);
void conn_insert(conn head, conn c);

int count_cur_conns();
unsigned int count_tot_conns();
int count_cur_producers();
int count_cur_workers();

void conn_set_producer(conn c);
void conn_set_worker(conn c);

#endif /*conn_h*/
