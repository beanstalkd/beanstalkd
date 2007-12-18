/* prot.h - protocol implementation header */

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

#ifndef prot_h
#define prot_h

#include "job.h"
#include "conn.h"

#define CONSTSTRLEN(m) (sizeof(m) - 1)

/* space for 16 Mi jobs */
#define HEAP_SIZE 16 * 1024 * 1024

#define URGENT_THRESHOLD 1024

/* measured in seconds */
#define RESERVATION_TIMEOUT 120

#define MSG_RESERVED "RESERVED"

#define CMD_PUT "put "
#define CMD_PEEK "peek"
#define CMD_PEEKJOB "peek "
#define CMD_RESERVE "reserve"
#define CMD_DELETE "delete "
#define CMD_RELEASE "release "
#define CMD_BURY "bury "
#define CMD_KICK "kick "
#define CMD_STATS "stats"
#define CMD_JOBSTATS "stats "

#define CMD_PEEK_LEN CONSTSTRLEN(CMD_PEEK)
#define CMD_PEEKJOB_LEN CONSTSTRLEN(CMD_PEEKJOB)
#define CMD_RESERVE_LEN CONSTSTRLEN(CMD_RESERVE)
#define CMD_DELETE_LEN CONSTSTRLEN(CMD_DELETE)
#define CMD_RELEASE_LEN CONSTSTRLEN(CMD_RELEASE)
#define CMD_BURY_LEN CONSTSTRLEN(CMD_BURY)
#define CMD_KICK_LEN CONSTSTRLEN(CMD_KICK)
#define CMD_STATS_LEN CONSTSTRLEN(CMD_STATS)
#define CMD_JOBSTATS_LEN CONSTSTRLEN(CMD_JOBSTATS)

#define MSG_FOUND "FOUND"
#define MSG_NOTFOUND "NOT_FOUND\r\n"
#define MSG_DELETED "DELETED\r\n"
#define MSG_RELEASED "RELEASED\r\n"
#define MSG_BURIED "BURIED\r\n"
#define MSG_INSERTED_FMT "INSERTED %llu\r\n"

#define MSG_NOTFOUND_LEN CONSTSTRLEN(MSG_NOTFOUND)
#define MSG_DELETED_LEN CONSTSTRLEN(MSG_DELETED)
#define MSG_RELEASED_LEN CONSTSTRLEN(MSG_RELEASED)
#define MSG_BURIED_LEN CONSTSTRLEN(MSG_BURIED)

#define STATS_FMT "---\n" \
    "current-jobs-urgent: %u\n" \
    "current-jobs-ready: %u\n" \
    "current-jobs-reserved: %u\n" \
    "current-jobs-delayed: %u\n" \
    "current-jobs-buried: %u\n" \
    "limit-max-jobs-ready: %u\n" \
    "cmd-put: %llu\n" \
    "cmd-peek: %llu\n" \
    "cmd-reserve: %llu\n" \
    "cmd-delete: %llu\n" \
    "cmd-release: %llu\n" \
    "cmd-bury: %llu\n" \
    "cmd-kick: %llu\n" \
    "cmd-stats: %llu\n" \
    "job-timeouts: %llu\n" \
    "total-jobs: %llu\n" \
    "current-connections: %u\n" \
    "current-producers: %u\n" \
    "current-workers: %u\n" \
    "current-waiting: %u\n" \
    "total-connections: %u\n" \
    "pid: %u\n" \
    "version: %s\n" \
    "rusage-utime: %d.%06d\n" \
    "rusage-stime: %d.%06d\n" \
    "uptime: %u\n" \
    "\r\n"

#define JOB_STATS_FMT "---\n" \
    "id: %llu\n" \
    "state: %s\n" \
    "age: %u\n" \
    "delay: %u\n" \
    "time-left: %u\n" \
    "timeouts: %u\n" \
    "releases: %u\n" \
    "buries: %u\n" \
    "kicks: %u\n" \
    "\r\n"

void prot_init();

void reply_job(conn c, job j, const char *word);

conn remove_waiting_conn(conn c);

int enqueue_job(job j, unsigned int delay);
void bury_job(job j);

void enter_drain_mode(int sig);
void h_accept(const int fd, const short which, struct event *ev);

#endif /*prot_h*/
