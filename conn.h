/* conn.h - network connection state */

#ifndef conn_h
#define conn_h

#include "event.h"
#include "job.h"

#define STATE_WANTCOMMAND 0
#define STATE_WANTDATA 1
#define STATE_WRITE 2
#define STATE_SENDWORD 3

/* a command can be at most LINE_BUF_SIZE chars, including "\r\n" */
#define LINE_BUF_SIZE 50

#define OP_UNKNOWN -1
#define OP_PUT 0
#define OP_PEEK 1
#define OP_RESERVE 2
#define OP_DONE 3
#define OP_STATS 4

typedef struct conn {
    int fd;
    char state;
    struct event evq;
    char cmd[LINE_BUF_SIZE]; /* this string is NOT NUL-terminated */
    int cmd_len;
    int cmd_read;
    const char *reply;
    int reply_len;
    int reply_sent;
    job job;
} *conn;

conn make_conn(int fd, char start_state);

int conn_update_evq(conn c, const int flags, evh handler);

void conn_close(conn c);

#endif /*conn_h*/
