/* conn.h - network connection state */

/* Copyright (C) 2007 Keith Rarick and Philotic Inc.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef conn_h
#define conn_h

#include "event.h"
#include "ms.h"
#include "tube.h"
#include "job.h"

/* A command can be at most LINE_BUF_SIZE chars, including "\r\n". This value
 * MUST be enough to hold the longest possible command or reply line, which is
 * currently "USING a{200}\r\n". */
#define LINE_BUF_SIZE 208

/* CONN_TYPE_* are bit masks */
#define CONN_TYPE_PRODUCER 1
#define CONN_TYPE_WORKER 2
#define CONN_TYPE_WAITING 4

#define conn_waiting(c) ((c)->type & CONN_TYPE_WAITING)

typedef struct conn *conn;

struct conn {
    conn prev, next; /* linked list of connections */
    int fd;
    char state;
    char type;
    struct event evq;
    int pending_timeout;

    /* we cannot share this buffer with the reply line because we might read in
     * command line data for a subsequent command, and we need to store it
     * here. */
    char cmd[LINE_BUF_SIZE]; /* this string is NOT NUL-terminated */
    int cmd_len;
    int cmd_read;
    const char *reply;
    int reply_len;
    int reply_sent;
    char reply_buf[LINE_BUF_SIZE]; /* this string IS NUL-terminated */

    /* A job to be read from the client. */
    job in_job;

    /* Memoization of the soonest job */
    job soonest_job;

    /* How many bytes of in_job->body have been read so far. If in_job is NULL
     * while in_job_read is nonzero, we are in bit bucket mode and
     * in_job_read's meaning is inverted -- then it counts the bytes that
     * remain to be thrown away. */
    int in_job_read;

    job out_job;
    int out_job_sent;
    struct job reserved_jobs; /* doubly-linked list header */
    tube use;
    struct ms watch;
};

conn make_conn(int fd, char start_state, tube use, tube watch);

int conn_set_evq(conn c, const int events, evh handler);
int conn_update_evq(conn c, const int flags);

void conn_close(conn c);

conn conn_remove(conn c);
void conn_insert(conn head, conn c);

int count_cur_conns();
unsigned int count_tot_conns();
int count_cur_producers();
int count_cur_workers();

void conn_set_producer(conn c);
void conn_set_worker(conn c);

job soonest_job(conn c);
int has_reserved_this_job(conn c, job j);
int conn_has_close_deadline(conn c);
int conn_ready(conn c);

#endif /*conn_h*/
