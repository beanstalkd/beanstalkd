/* prot.c - protocol implementation */

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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <stdarg.h>

#include "stat.h"
#include "prot.h"
#include "pq.h"
#include "ms.h"
#include "job.h"
#include "tube.h"
#include "conn.h"
#include "util.h"
#include "net.h"
#include "binlog.h"
#include "version.h"

/* job body cannot be greater than this many bytes long */
size_t job_data_size_limit = ((1 << 16) - 1);

#define NAME_CHARS \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefghijklmnopqrstuvwxyz" \
    "0123456789-+/;.$()"

#define CMD_PUT "put "
#define CMD_PEEKJOB "peek "
#define CMD_PEEK_READY "peek-ready"
#define CMD_PEEK_DELAYED "peek-delayed"
#define CMD_PEEK_BURIED "peek-buried"
#define CMD_RESERVE "reserve"
#define CMD_RESERVE_TIMEOUT "reserve-with-timeout "
#define CMD_DELETE "delete "
#define CMD_RELEASE "release "
#define CMD_BURY "bury "
#define CMD_KICK "kick "
#define CMD_STATS "stats"
#define CMD_JOBSTATS "stats-job "
#define CMD_USE "use "
#define CMD_WATCH "watch "
#define CMD_IGNORE "ignore "
#define CMD_LIST_TUBES "list-tubes"
#define CMD_LIST_TUBE_USED "list-tube-used"
#define CMD_LIST_TUBES_WATCHED "list-tubes-watched"
#define CMD_STATS_TUBE "stats-tube "

#define CONSTSTRLEN(m) (sizeof(m) - 1)

#define CMD_PEEK_READY_LEN CONSTSTRLEN(CMD_PEEK_READY)
#define CMD_PEEK_DELAYED_LEN CONSTSTRLEN(CMD_PEEK_DELAYED)
#define CMD_PEEK_BURIED_LEN CONSTSTRLEN(CMD_PEEK_BURIED)
#define CMD_PEEKJOB_LEN CONSTSTRLEN(CMD_PEEKJOB)
#define CMD_RESERVE_LEN CONSTSTRLEN(CMD_RESERVE)
#define CMD_RESERVE_TIMEOUT_LEN CONSTSTRLEN(CMD_RESERVE_TIMEOUT)
#define CMD_DELETE_LEN CONSTSTRLEN(CMD_DELETE)
#define CMD_RELEASE_LEN CONSTSTRLEN(CMD_RELEASE)
#define CMD_BURY_LEN CONSTSTRLEN(CMD_BURY)
#define CMD_KICK_LEN CONSTSTRLEN(CMD_KICK)
#define CMD_STATS_LEN CONSTSTRLEN(CMD_STATS)
#define CMD_JOBSTATS_LEN CONSTSTRLEN(CMD_JOBSTATS)
#define CMD_USE_LEN CONSTSTRLEN(CMD_USE)
#define CMD_WATCH_LEN CONSTSTRLEN(CMD_WATCH)
#define CMD_IGNORE_LEN CONSTSTRLEN(CMD_IGNORE)
#define CMD_LIST_TUBES_LEN CONSTSTRLEN(CMD_LIST_TUBES)
#define CMD_LIST_TUBE_USED_LEN CONSTSTRLEN(CMD_LIST_TUBE_USED)
#define CMD_LIST_TUBES_WATCHED_LEN CONSTSTRLEN(CMD_LIST_TUBES_WATCHED)
#define CMD_STATS_TUBE_LEN CONSTSTRLEN(CMD_STATS_TUBE)

#define MSG_FOUND "FOUND"
#define MSG_NOTFOUND "NOT_FOUND\r\n"
#define MSG_RESERVED "RESERVED"
#define MSG_DEADLINE_SOON "DEADLINE_SOON\r\n"
#define MSG_TIMED_OUT "TIMED_OUT\r\n"
#define MSG_DELETED "DELETED\r\n"
#define MSG_RELEASED "RELEASED\r\n"
#define MSG_BURIED "BURIED\r\n"
#define MSG_BURIED_FMT "BURIED %llu\r\n"
#define MSG_INSERTED_FMT "INSERTED %llu\r\n"
#define MSG_NOT_IGNORED "NOT_IGNORED\r\n"

#define MSG_NOTFOUND_LEN CONSTSTRLEN(MSG_NOTFOUND)
#define MSG_DELETED_LEN CONSTSTRLEN(MSG_DELETED)
#define MSG_RELEASED_LEN CONSTSTRLEN(MSG_RELEASED)
#define MSG_BURIED_LEN CONSTSTRLEN(MSG_BURIED)
#define MSG_NOT_IGNORED_LEN CONSTSTRLEN(MSG_NOT_IGNORED)

#define MSG_OUT_OF_MEMORY "OUT_OF_MEMORY\r\n"
#define MSG_INTERNAL_ERROR "INTERNAL_ERROR\r\n"
#define MSG_DRAINING "DRAINING\r\n"
#define MSG_BAD_FORMAT "BAD_FORMAT\r\n"
#define MSG_UNKNOWN_COMMAND "UNKNOWN_COMMAND\r\n"
#define MSG_EXPECTED_CRLF "EXPECTED_CRLF\r\n"
#define MSG_JOB_TOO_BIG "JOB_TOO_BIG\r\n"

#define STATE_WANTCOMMAND 0
#define STATE_WANTDATA 1
#define STATE_SENDJOB 2
#define STATE_SENDWORD 3
#define STATE_WAIT 4
#define STATE_BITBUCKET 5

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
#define OP_PEEK_BURIED 10
#define OP_USE 11
#define OP_WATCH 12
#define OP_IGNORE 13
#define OP_LIST_TUBES 14
#define OP_LIST_TUBE_USED 15
#define OP_LIST_TUBES_WATCHED 16
#define OP_STATS_TUBE 17
#define OP_PEEK_READY 18
#define OP_PEEK_DELAYED 19
#define OP_RESERVE_TIMEOUT 20
#define TOTAL_OPS 21

#define STATS_FMT "---\n" \
    "current-jobs-urgent: %u\n" \
    "current-jobs-ready: %u\n" \
    "current-jobs-reserved: %u\n" \
    "current-jobs-delayed: %u\n" \
    "current-jobs-buried: %u\n" \
    "cmd-put: %llu\n" \
    "cmd-peek: %llu\n" \
    "cmd-peek-ready: %llu\n" \
    "cmd-peek-delayed: %llu\n" \
    "cmd-peek-buried: %llu\n" \
    "cmd-reserve: %llu\n" \
    "cmd-reserve-with-timeout: %llu\n" \
    "cmd-delete: %llu\n" \
    "cmd-release: %llu\n" \
    "cmd-use: %llu\n" \
    "cmd-watch: %llu\n" \
    "cmd-ignore: %llu\n" \
    "cmd-bury: %llu\n" \
    "cmd-kick: %llu\n" \
    "cmd-stats: %llu\n" \
    "cmd-stats-job: %llu\n" \
    "cmd-stats-tube: %llu\n" \
    "cmd-list-tubes: %llu\n" \
    "cmd-list-tube-used: %llu\n" \
    "cmd-list-tubes-watched: %llu\n" \
    "job-timeouts: %llu\n" \
    "total-jobs: %llu\n" \
    "max-job-size: %zu\n" \
    "current-tubes: %zu\n" \
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

#define STATS_TUBE_FMT "---\n" \
    "name: %s\n" \
    "current-jobs-urgent: %u\n" \
    "current-jobs-ready: %u\n" \
    "current-jobs-reserved: %u\n" \
    "current-jobs-delayed: %u\n" \
    "current-jobs-buried: %u\n" \
    "total-jobs: %llu\n" \
    "current-using: %u\n" \
    "current-watching: %u\n" \
    "current-waiting: %u\n" \
    "\r\n"

#define JOB_STATS_FMT "---\n" \
    "id: %llu\n" \
    "tube: %s\n" \
    "state: %s\n" \
    "pri: %u\n" \
    "age: %u\n" \
    "delay: %u\n" \
    "ttr: %u\n" \
    "time-left: %u\n" \
    "timeouts: %u\n" \
    "releases: %u\n" \
    "buries: %u\n" \
    "kicks: %u\n" \
    "\r\n"

/* this number is pretty arbitrary */
#define BUCKET_BUF_SIZE 1024

static char bucket[BUCKET_BUF_SIZE];

static unsigned int ready_ct = 0;
static struct stats global_stat = {0, 0, 0, 0, 0};

static tube default_tube;

static int drain_mode = 0;
static time_t start_time;
static unsigned long long int op_ct[TOTAL_OPS], timeout_ct = 0;


/* Doubly-linked list of connections with at least one reserved job. */
static struct conn running = { &running, &running, 0 };

#ifdef DEBUG
static const char * op_names[] = {
    "<unknown>",
    CMD_PUT,
    CMD_PEEKJOB,
    CMD_RESERVE,
    CMD_DELETE,
    CMD_RELEASE,
    CMD_BURY,
    CMD_KICK,
    CMD_STATS,
    CMD_JOBSTATS,
    CMD_PEEK_BURIED,
    CMD_USE,
    CMD_WATCH,
    CMD_IGNORE,
    CMD_LIST_TUBES,
    CMD_LIST_TUBE_USED,
    CMD_LIST_TUBES_WATCHED,
    CMD_STATS_TUBE,
    CMD_PEEK_READY,
    CMD_PEEK_DELAYED,
    CMD_RESERVE_TIMEOUT
};
#endif

static int
buried_job_p(tube t)
{
    return job_list_any_p(&t->buried);
}

static void
reply(conn c, const char *line, int len, int state)
{
    int r;

    if (!c) return;

    r = conn_update_evq(c, EV_WRITE | EV_PERSIST);
    if (r == -1) return twarnx("conn_update_evq() failed"), conn_close(c);

    c->reply = line;
    c->reply_len = len;
    c->reply_sent = 0;
    c->state = state;
    dprintf("sending reply: %.*s", len, line);
}

#define reply_msg(c,m) reply((c),(m),CONSTSTRLEN(m),STATE_SENDWORD)

#define reply_serr(c,e) (twarnx("server error: %s",(e)),\
                         reply_msg((c),(e)))

static void
reply_line(conn c, int state, const char *fmt, ...)
{
    int r;
    va_list ap;

    va_start(ap, fmt);
    r = vsnprintf(c->reply_buf, LINE_BUF_SIZE, fmt, ap);
    va_end(ap);

    /* Make sure the buffer was big enough. If not, we have a bug. */
    if (r >= LINE_BUF_SIZE) return reply_serr(c, MSG_INTERNAL_ERROR);

    return reply(c, c->reply_buf, r, state);
}

static void
reply_job(conn c, job j, const char *word)
{
    /* tell this connection which job to send */
    c->out_job = j;
    c->out_job_sent = 0;

    return reply_line(c, STATE_SENDJOB, "%s %llu %u\r\n",
                      word, j->id, j->body_size - 2);
}

conn
remove_waiting_conn(conn c)
{
    tube t;
    size_t i;

    if (!conn_waiting(c)) return NULL;

    c->type &= ~CONN_TYPE_WAITING;
    global_stat.waiting_ct--;
    for (i = 0; i < c->watch.used; i++) {
        t = c->watch.items[i];
        t->stat.waiting_ct--;
        ms_remove(&t->waiting, c);
    }
    return c;
}

static void
reserve_job(conn c, job j)
{
    j->deadline = time(NULL) + j->ttr;
    global_stat.reserved_ct++; /* stats */
    j->tube->stat.reserved_ct++;
    conn_insert(&running, c);
    j->state = JOB_STATE_RESERVED;
    job_insert(&c->reserved_jobs, j);
    j->reserver=c;
    if (c->soonest_job && j->deadline < c->soonest_job->deadline) {
      c->soonest_job = j;
    }
    return reply_job(c, j, MSG_RESERVED);
}

static job
next_eligible_job()
{
    tube t;
    size_t i;
    job j = NULL, candidate;

    dprintf("tubes.used = %zu\n", tubes.used);
    for (i = 0; i < tubes.used; i++) {
        t = tubes.items[i];
        dprintf("for %s t->waiting.used=%zu t->ready.used=%d\n",
                t->name, t->waiting.used, t->ready.used);
        if (t->waiting.used && t->ready.used) {
            candidate = pq_peek(&t->ready);
            if (!j || candidate->id < j->id) j = candidate;
        }
        dprintf("i = %zu, tubes.used = %zu\n", i, tubes.used);
    }

    return j;
}

static void
process_queue()
{
    job j;

    dprintf("processing queue\n");
    while ((j = next_eligible_job())) {
        dprintf("got eligible job %llu in %s\n", j->id, j->tube->name);
        j = pq_take(&j->tube->ready);
        ready_ct--;
        if (j->pri < URGENT_THRESHOLD) {
            global_stat.urgent_ct--;
            j->tube->stat.urgent_ct--;
        }
        reserve_job(remove_waiting_conn(ms_take(&j->tube->waiting)), j);
    }
}

static job
delay_q_peek()
{
    int i;
    tube t;
    job j = NULL, nj;

    for (i = 0; i < tubes.used; i++) {
        t = tubes.items[i];
        nj = pq_peek(&t->delay);
        if (!nj) continue;
        if (!j || nj->deadline < j->deadline) j = nj;
    }

    return j;
}

static void
set_main_delay_timeout()
{
    job j;

    set_main_timeout((j = delay_q_peek()) ? j->deadline : 0);
}

static int
enqueue_job(job j, unsigned int delay)
{
    int r;

    j->reserver = NULL;
    if (delay) {
        j->deadline = time(NULL) + delay;
        r = pq_give(&j->tube->delay, j);
        if (!r) return 0;
        j->state = JOB_STATE_DELAYED;
        set_main_delay_timeout();
    } else {
        r = pq_give(&j->tube->ready, j);
        if (!r) return 0;
        j->state = JOB_STATE_READY;
        ready_ct++;
        if (j->pri < URGENT_THRESHOLD) {
            global_stat.urgent_ct++;
            j->tube->stat.urgent_ct++;
        }
    }
    binlog_write_job(j);
    process_queue();
    return 1;
}

static void
bury_job(job j)
{
    job_insert(&j->tube->buried, j);
    global_stat.buried_ct++;
    j->tube->stat.buried_ct++;
    j->state = JOB_STATE_BURIED;
    j->reserver=NULL;
    j->bury_ct++;
    binlog_write_job(j);
}

void
enqueue_reserved_jobs(conn c)
{
    int r;
    job j;

    while (job_list_any_p(&c->reserved_jobs)) {
        j = job_remove(c->reserved_jobs.next);
        r = enqueue_job(j, 0);
        if (!r) bury_job(j);
        global_stat.reserved_ct--;
        j->tube->stat.reserved_ct--;
        c->soonest_job = NULL;
        if (!job_list_any_p(&c->reserved_jobs)) conn_remove(c);
    }
}

static job
delay_q_take()
{
    job j = delay_q_peek();
    return j ? pq_take(&j->tube->delay) : NULL;
}

static job
remove_this_buried_job(job j)
{
    j = job_remove(j);
    if (j) {
        global_stat.buried_ct--;
        j->tube->stat.buried_ct--;
    }
    return j;
}

static int
kick_buried_job(tube t)
{
    int r;
    job j;

    if (!buried_job_p(t)) return 0;
    j = remove_this_buried_job(t->buried.next);
    j->kick_ct++;
    r = enqueue_job(j, 0);
    if (r) return 1;

    /* ready queue is full, so bury it */
    bury_job(j);
    return 0;
}

static unsigned int
get_delayed_job_ct()
{
    tube t;
    size_t i;
    unsigned int count = 0;

    for (i = 0; i < tubes.used; i++) {
        t = tubes.items[i];
        count += pq_used(&t->delay);
    }
    return count;
}

static int
kick_delayed_job(tube t)
{
    int r;
    job j;

    j = pq_take(&t->delay);
    if (!j) return 0;
    j->kick_ct++;
    r = enqueue_job(j, 0);
    if (r) return 1;

    /* ready queue is full, so delay it again */
    r = enqueue_job(j, j->delay);
    if (r) return 0;

    /* last resort */
    bury_job(j);
    return 0;
}

/* return the number of jobs successfully kicked */
static unsigned int
kick_buried_jobs(tube t, unsigned int n)
{
    unsigned int i;
    for (i = 0; (i < n) && kick_buried_job(t); ++i);
    return i;
}

/* return the number of jobs successfully kicked */
static unsigned int
kick_delayed_jobs(tube t, unsigned int n)
{
    unsigned int i;
    for (i = 0; (i < n) && kick_delayed_job(t); ++i);
    return i;
}

static unsigned int
kick_jobs(tube t, unsigned int n)
{
    if (buried_job_p(t)) return kick_buried_jobs(t, n);
    return kick_delayed_jobs(t, n);
}

static job
find_buried_job(unsigned long long int id)
{
    job j = job_find(id);
    return (j && j->state == JOB_STATE_BURIED) ? j : NULL;
}

static job
remove_buried_job(unsigned long long int id)
{
    return remove_this_buried_job(find_buried_job(id));
}

static void
enqueue_waiting_conn(conn c)
{
    tube t;
    size_t i;

    global_stat.waiting_ct++;
    c->type |= CONN_TYPE_WAITING;
    for (i = 0; i < c->watch.used; i++) {
        t = c->watch.items[i];
        t->stat.waiting_ct++;
        ms_append(&t->waiting, c);
    }
}

static job
find_reserved_job_in_conn(conn c, unsigned long long int id)
{
    job j = job_find(id);
    return (j && j->reserver == c && j->state == JOB_STATE_RESERVED) ? j : NULL;
}

static job
peek_job(unsigned long long int id)
{
    return job_find(id);
}

static void
check_err(conn c, const char *s)
{
    if (errno == EAGAIN) return;
    if (errno == EINTR) return;
    if (errno == EWOULDBLOCK) return;

    twarn("%s", s);
    conn_close(c);
    return;
}

/* Scan the given string for the sequence "\r\n" and return the line length.
 * Always returns at least 2 if a match is found. Returns 0 if no match. */
static int
scan_line_end(const char *s, int size)
{
    char *match;

    match = memchr(s, '\r', size - 1);
    if (!match) return 0;

    /* this is safe because we only scan size - 1 chars above */
    if (match[1] == '\n') return match - s + 2;

    return 0;
}

static int
cmd_len(conn c)
{
    return scan_line_end(c->cmd, c->cmd_read);
}

/* parse the command line */
static int
which_cmd(conn c)
{
#define TEST_CMD(s,c,o) if (strncmp((s), (c), CONSTSTRLEN(c)) == 0) return (o);
    TEST_CMD(c->cmd, CMD_PUT, OP_PUT);
    TEST_CMD(c->cmd, CMD_PEEKJOB, OP_PEEKJOB);
    TEST_CMD(c->cmd, CMD_PEEK_READY, OP_PEEK_READY);
    TEST_CMD(c->cmd, CMD_PEEK_DELAYED, OP_PEEK_DELAYED);
    TEST_CMD(c->cmd, CMD_PEEK_BURIED, OP_PEEK_BURIED);
    TEST_CMD(c->cmd, CMD_RESERVE_TIMEOUT, OP_RESERVE_TIMEOUT);
    TEST_CMD(c->cmd, CMD_RESERVE, OP_RESERVE);
    TEST_CMD(c->cmd, CMD_DELETE, OP_DELETE);
    TEST_CMD(c->cmd, CMD_RELEASE, OP_RELEASE);
    TEST_CMD(c->cmd, CMD_BURY, OP_BURY);
    TEST_CMD(c->cmd, CMD_KICK, OP_KICK);
    TEST_CMD(c->cmd, CMD_JOBSTATS, OP_JOBSTATS);
    TEST_CMD(c->cmd, CMD_STATS_TUBE, OP_STATS_TUBE);
    TEST_CMD(c->cmd, CMD_STATS, OP_STATS);
    TEST_CMD(c->cmd, CMD_USE, OP_USE);
    TEST_CMD(c->cmd, CMD_WATCH, OP_WATCH);
    TEST_CMD(c->cmd, CMD_IGNORE, OP_IGNORE);
    TEST_CMD(c->cmd, CMD_LIST_TUBES_WATCHED, OP_LIST_TUBES_WATCHED);
    TEST_CMD(c->cmd, CMD_LIST_TUBE_USED, OP_LIST_TUBE_USED);
    TEST_CMD(c->cmd, CMD_LIST_TUBES, OP_LIST_TUBES);
    return OP_UNKNOWN;
}

/* Copy up to body_size trailing bytes into the job, then the rest into the cmd
 * buffer. If c->in_job exists, this assumes that c->in_job->body is empty.
 * This function is idempotent(). */
static void
fill_extra_data(conn c)
{
    int extra_bytes, job_data_bytes = 0, cmd_bytes;

    if (!c->fd) return; /* the connection was closed */
    if (!c->cmd_len) return; /* we don't have a complete command */

    /* how many extra bytes did we read? */
    extra_bytes = c->cmd_read - c->cmd_len;

    /* how many bytes should we put into the job body? */
    if (c->in_job) {
        job_data_bytes = min(extra_bytes, c->in_job->body_size);
        memcpy(c->in_job->body, c->cmd + c->cmd_len, job_data_bytes);
        c->in_job_read = job_data_bytes;
    } else if (c->in_job_read) {
        /* we are in bit-bucket mode, throwing away data */
        job_data_bytes = min(extra_bytes, c->in_job_read);
        c->in_job_read -= job_data_bytes;
    }

    /* how many bytes are left to go into the future cmd? */
    cmd_bytes = extra_bytes - job_data_bytes;
    memmove(c->cmd, c->cmd + c->cmd_len + job_data_bytes, cmd_bytes);
    c->cmd_read = cmd_bytes;
    c->cmd_len = 0; /* we no longer know the length of the new command */
}

static void
enqueue_incoming_job(conn c)
{
    int r;
    job j = c->in_job;

    c->in_job = NULL; /* the connection no longer owns this job */
    c->in_job_read = 0;

    /* check if the trailer is present and correct */
    if (memcmp(j->body + j->body_size - 2, "\r\n", 2)) {
        job_free(j);
        return reply_msg(c, MSG_EXPECTED_CRLF);
    }

    if (drain_mode) {
        job_free(j);
        return reply_serr(c, MSG_DRAINING);
    }

    /* we have a complete job, so let's stick it in the pqueue */
    r = enqueue_job(j, j->delay);
    op_ct[OP_PUT]++; /* stats */
    global_stat.total_jobs_ct++;
    j->tube->stat.total_jobs_ct++;

    if (r) return reply_line(c, STATE_SENDWORD, MSG_INSERTED_FMT, j->id);

    /* out of memory trying to grow the queue, so it gets buried */
    bury_job(j);
    reply_line(c, STATE_SENDWORD, MSG_BURIED_FMT, j->id);
}

static unsigned int
uptime()
{
    return time(NULL) - start_time;
}

static int
fmt_stats(char *buf, size_t size, void *x)
{
    struct rusage ru = {{0, 0}, {0, 0}};
    getrusage(RUSAGE_SELF, &ru); /* don't care if it fails */
    return snprintf(buf, size, STATS_FMT,
            global_stat.urgent_ct,
            ready_ct,
            global_stat.reserved_ct,
            get_delayed_job_ct(),
            global_stat.buried_ct,
            op_ct[OP_PUT],
            op_ct[OP_PEEKJOB],
            op_ct[OP_PEEK_READY],
            op_ct[OP_PEEK_DELAYED],
            op_ct[OP_PEEK_BURIED],
            op_ct[OP_RESERVE],
            op_ct[OP_RESERVE_TIMEOUT],
            op_ct[OP_DELETE],
            op_ct[OP_RELEASE],
            op_ct[OP_USE],
            op_ct[OP_WATCH],
            op_ct[OP_IGNORE],
            op_ct[OP_BURY],
            op_ct[OP_KICK],
            op_ct[OP_STATS],
            op_ct[OP_JOBSTATS],
            op_ct[OP_STATS_TUBE],
            op_ct[OP_LIST_TUBES],
            op_ct[OP_LIST_TUBE_USED],
            op_ct[OP_LIST_TUBES_WATCHED],
            timeout_ct,
            global_stat.total_jobs_ct,
            job_data_size_limit,
            tubes.used,
            count_cur_conns(),
            count_cur_producers(),
            count_cur_workers(),
            global_stat.waiting_ct,
            count_tot_conns(),
            getpid(),
            BEAN_VERSION,
            (int) ru.ru_utime.tv_sec, (int) ru.ru_utime.tv_usec,
            (int) ru.ru_stime.tv_sec, (int) ru.ru_stime.tv_usec,
            uptime());

}

/* Read a priority value from the given buffer and place it in pri.
 * Update end to point to the address after the last character consumed.
 * Pri and end can be NULL. If they are both NULL, read_pri() will do the
 * conversion and return the status code but not update any values. This is an
 * easy way to check for errors.
 * If end is NULL, read_pri will also check that the entire input string was
 * consumed and return an error code otherwise.
 * Return 0 on success, or nonzero on failure.
 * If a failure occurs, pri and end are not modified. */
static int
read_pri(unsigned int *pri, const char *buf, char **end)
{
    char *tend;
    unsigned int tpri;

    errno = 0;
    tpri = strtoul(buf, &tend, 10);
    if (tend == buf) return -1;
    if (errno && errno != ERANGE) return -1;
    if (!end && tend[0] != '\0') return -1;

    if (pri) *pri = tpri;
    if (end) *end = tend;
    return 0;
}

/* Read a delay value from the given buffer and place it in delay.
 * The interface and behavior are the same as in read_pri(). */
static int
read_delay(unsigned int *delay, const char *buf, char **end)
{
    return read_pri(delay, buf, end);
}

/* Read a timeout value from the given buffer and place it in ttr.
 * The interface and behavior are the same as in read_pri(). */
static int
read_ttr(unsigned int *ttr, const char *buf, char **end)
{
    return read_pri(ttr, buf, end);
}

static void
wait_for_job(conn c, int timeout)
{
    int r;

    c->state = STATE_WAIT;
    enqueue_waiting_conn(c);

    /* Set the pending timeout to the requested timeout amount */
    c->pending_timeout = timeout;

    /* this conn is waiting, but we want to know if they hang up */
    r = conn_update_evq(c, EV_READ | EV_PERSIST);
    if (r == -1) return twarnx("update events failed"), conn_close(c);
}

typedef int(*fmt_fn)(char *, size_t, void *);

static void
do_stats(conn c, fmt_fn fmt, void *data)
{
    int r, stats_len;

    /* first, measure how big a buffer we will need */
    stats_len = fmt(NULL, 0, data) + 16;

    c->out_job = allocate_job(stats_len); /* fake job to hold stats data */
    if (!c->out_job) return reply_serr(c, MSG_OUT_OF_MEMORY);

    /* now actually format the stats data */
    r = fmt(c->out_job->body, stats_len, data);
    /* and set the actual body size */
    c->out_job->body_size = r;
    if (r > stats_len) return reply_serr(c, MSG_INTERNAL_ERROR);

    c->out_job_sent = 0;
    return reply_line(c, STATE_SENDJOB, "OK %d\r\n", r - 2);
}

static void
do_list_tubes(conn c, ms l)
{
    char *buf;
    tube t;
    size_t i, resp_z;

    /* first, measure how big a buffer we will need */
    resp_z = 6; /* initial "---\n" and final "\r\n" */
    for (i = 0; i < l->used; i++) {
        t = l->items[i];
        resp_z += 3 + strlen(t->name); /* including "- " and "\n" */
    }

    c->out_job = allocate_job(resp_z); /* fake job to hold response data */
    if (!c->out_job) return reply_serr(c, MSG_OUT_OF_MEMORY);

    /* now actually format the response */
    buf = c->out_job->body;
    buf += snprintf(buf, 5, "---\n");
    for (i = 0; i < l->used; i++) {
        t = l->items[i];
        buf += snprintf(buf, 4 + strlen(t->name), "- %s\n", t->name);
    }
    buf[0] = '\r';
    buf[1] = '\n';

    c->out_job_sent = 0;
    return reply_line(c, STATE_SENDJOB, "OK %d\r\n", resp_z - 2);
}

static int
fmt_job_stats(char *buf, size_t size, job j)
{
    time_t t;

    t = time(NULL);
    return snprintf(buf, size, JOB_STATS_FMT,
            j->id,
            j->tube->name,
            job_state(j),
            j->pri,
            (unsigned int) (t - j->creation),
            j->delay,
            j->ttr,
            (unsigned int) (j->deadline - t),
            j->timeout_ct,
            j->release_ct,
            j->bury_ct,
            j->kick_ct);
}

static int
fmt_stats_tube(char *buf, size_t size, tube t)
{
    return snprintf(buf, size, STATS_TUBE_FMT,
            t->name,
            t->stat.urgent_ct,
            t->ready.used,
            t->stat.reserved_ct,
            pq_used(&t->delay),
            t->stat.buried_ct,
            t->stat.total_jobs_ct,
            t->using_ct,
            t->watching_ct,
            t->stat.waiting_ct);
}

static void
maybe_enqueue_incoming_job(conn c)
{
    job j = c->in_job;

    /* do we have a complete job? */
    if (c->in_job_read == j->body_size) return enqueue_incoming_job(c);

    /* otherwise we have incomplete data, so just keep waiting */
    c->state = STATE_WANTDATA;
}

/* j can be NULL */
static job
remove_this_reserved_job(conn c, job j)
{
    j = job_remove(j);
    if (j) {
        global_stat.reserved_ct--;
        j->tube->stat.reserved_ct--;
        j->reserver=NULL;
    }
    c->soonest_job = NULL;
    if (!job_list_any_p(&c->reserved_jobs)) conn_remove(c);
    return j;
}

static job
remove_reserved_job(conn c, unsigned long long int id)
{
    return remove_this_reserved_job(c, find_reserved_job_in_conn(c, id));
}

static int
name_is_ok(const char *name, size_t max)
{
    size_t len = strlen(name);
    return len > 0 && len <= max &&
        strspn(name, NAME_CHARS) == len && name[0] != '-';
}

void
prot_remove_tube(tube t)
{
    ms_remove(&tubes, t);
}

static void
dispatch_cmd(conn c)
{
    int r, i, timeout = -1;
    unsigned int count;
    job j;
    unsigned char type;
    char *size_buf, *delay_buf, *ttr_buf, *pri_buf, *end_buf, *name;
    unsigned int pri, delay, ttr, body_size;
    unsigned long long int id;
    tube t = NULL;

    /* NUL-terminate this string so we can use strtol and friends */
    c->cmd[c->cmd_len - 2] = '\0';

    /* check for possible maliciousness */
    if (strlen(c->cmd) != c->cmd_len - 2) {
        return reply_msg(c, MSG_BAD_FORMAT);
    }

    type = which_cmd(c);
    dprintf("got %s command: \"%s\"\n", op_names[(int) type], c->cmd);

    switch (type) {
    case OP_PUT:
        r = read_pri(&pri, c->cmd + 4, &delay_buf);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);

        r = read_delay(&delay, delay_buf, &ttr_buf);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);

        r = read_ttr(&ttr, ttr_buf, &size_buf);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);

        errno = 0;
        body_size = strtoul(size_buf, &end_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);

        if (body_size > job_data_size_limit) {
            return reply_msg(c, MSG_JOB_TOO_BIG);
        }

        /* don't allow trailing garbage */
        if (end_buf[0] != '\0') return reply_msg(c, MSG_BAD_FORMAT);

        conn_set_producer(c);

        c->in_job = make_job(pri, delay, ttr ? : 1, body_size + 2, c->use);

        /* OOM? */
        if (!c->in_job) {
            /* throw away the job body and respond with OUT_OF_MEMORY */

            /* Invert the meaning of in_job_read while throwing away data -- it
             * counts the bytes that remain to be thrown away. */
            c->in_job_read = body_size + 2;
            fill_extra_data(c);

            if (c->in_job_read == 0) return reply_serr(c, MSG_OUT_OF_MEMORY);

            c->state = STATE_BITBUCKET;
            return;
        }

        fill_extra_data(c);

        /* it's possible we already have a complete job */
        maybe_enqueue_incoming_job(c);

        break;
    case OP_PEEK_READY:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_PEEK_READY_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }
        op_ct[type]++;

        j = job_copy(pq_peek(&c->use->ready));

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        reply_job(c, j, MSG_FOUND);
        break;
    case OP_PEEK_DELAYED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_PEEK_DELAYED_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }
        op_ct[type]++;

        j = job_copy(pq_peek(&c->use->delay));

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        reply_job(c, j, MSG_FOUND);
        break;
    case OP_PEEK_BURIED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_PEEK_BURIED_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }
        op_ct[type]++;

        j = job_copy(buried_job_p(c->use)? j = c->use->buried.next : NULL);

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        reply_job(c, j, MSG_FOUND);
        break;
    case OP_PEEKJOB:
        errno = 0;
        id = strtoull(c->cmd + CMD_PEEKJOB_LEN, &end_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        /* So, peek is annoying, because some other connection might free the
         * job while we are still trying to write it out. So we copy it and
         * then free the copy when it's done sending. */
        j = job_copy(peek_job(id));

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        reply_job(c, j, MSG_FOUND);
        break;
    case OP_RESERVE_TIMEOUT:
        errno = 0;
        timeout = strtol(c->cmd + CMD_RESERVE_TIMEOUT_LEN, &end_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);
    case OP_RESERVE: /* FALLTHROUGH */
        /* don't allow trailing garbage */
        if (type == OP_RESERVE && c->cmd_len != CMD_RESERVE_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }

        op_ct[type]++;
        conn_set_worker(c);

        if (conn_has_close_deadline(c) && !conn_ready(c)) {
            return reply_msg(c, MSG_DEADLINE_SOON);
        }

        /* try to get a new job for this guy */
        wait_for_job(c, timeout);
        process_queue();
        break;
    case OP_DELETE:
        errno = 0;
        id = strtoull(c->cmd + CMD_DELETE_LEN, &end_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        j = remove_reserved_job(c, id) ? : remove_buried_job(id);

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        j->state = JOB_STATE_INVALID;
        binlog_write_job(j);
        job_free(j);

        reply(c, MSG_DELETED, MSG_DELETED_LEN, STATE_SENDWORD);
        break;
    case OP_RELEASE:
        errno = 0;
        id = strtoull(c->cmd + CMD_RELEASE_LEN, &pri_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);

        r = read_pri(&pri, pri_buf, &delay_buf);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);

        r = read_delay(&delay, delay_buf, NULL);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        j = remove_reserved_job(c, id);

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        j->pri = pri;
        j->delay = delay;
        j->release_ct++;
        r = enqueue_job(j, delay);
        if (r) return reply(c, MSG_RELEASED, MSG_RELEASED_LEN, STATE_SENDWORD);

        /* out of memory trying to grow the queue, so it gets buried */
        bury_job(j);
        reply(c, MSG_BURIED, MSG_BURIED_LEN, STATE_SENDWORD);
        break;
    case OP_BURY:
        errno = 0;
        id = strtoull(c->cmd + CMD_BURY_LEN, &pri_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);

        r = read_pri(&pri, pri_buf, NULL);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        j = remove_reserved_job(c, id);

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        j->pri = pri;
        bury_job(j);
        reply(c, MSG_BURIED, MSG_BURIED_LEN, STATE_SENDWORD);
        break;
    case OP_KICK:
        errno = 0;
        count = strtoul(c->cmd + CMD_KICK_LEN, &end_buf, 10);
        if (end_buf == c->cmd + CMD_KICK_LEN) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);

        op_ct[type]++;

        i = kick_jobs(c->use, count);

        return reply_line(c, STATE_SENDWORD, "KICKED %u\r\n", i);
    case OP_STATS:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_STATS_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }

        op_ct[type]++;

        do_stats(c, fmt_stats, NULL);
        break;
    case OP_JOBSTATS:
        errno = 0;
        id = strtoull(c->cmd + CMD_JOBSTATS_LEN, &end_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);

        op_ct[type]++;

        j = peek_job(id);
        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        if (!j->tube) return reply_serr(c, MSG_INTERNAL_ERROR);
        do_stats(c, (fmt_fn) fmt_job_stats, j);
        break;
    case OP_STATS_TUBE:
        name = c->cmd + CMD_STATS_TUBE_LEN;
        if (!name_is_ok(name, 200)) return reply_msg(c, MSG_BAD_FORMAT);

        op_ct[type]++;

        t = tube_find(name);
        if (!t) return reply_msg(c, MSG_NOTFOUND);

        do_stats(c, (fmt_fn) fmt_stats_tube, t);
        t = NULL;
        break;
    case OP_LIST_TUBES:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_LIST_TUBES_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }

        op_ct[type]++;
        do_list_tubes(c, &tubes);
        break;
    case OP_LIST_TUBE_USED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_LIST_TUBE_USED_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }

        op_ct[type]++;
        reply_line(c, STATE_SENDWORD, "USING %s\r\n", c->use->name);
        break;
    case OP_LIST_TUBES_WATCHED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_LIST_TUBES_WATCHED_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }

        op_ct[type]++;
        do_list_tubes(c, &c->watch);
        break;
    case OP_USE:
        name = c->cmd + CMD_USE_LEN;
        if (!name_is_ok(name, 200)) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        TUBE_ASSIGN(t, tube_find_or_make(name));
        if (!t) return reply_serr(c, MSG_OUT_OF_MEMORY);

        c->use->using_ct--;
        TUBE_ASSIGN(c->use, t);
        TUBE_ASSIGN(t, NULL);
        c->use->using_ct++;

        reply_line(c, STATE_SENDWORD, "USING %s\r\n", c->use->name);
        break;
    case OP_WATCH:
        name = c->cmd + CMD_WATCH_LEN;
        if (!name_is_ok(name, 200)) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        TUBE_ASSIGN(t, tube_find_or_make(name));
        if (!t) return reply_serr(c, MSG_OUT_OF_MEMORY);

        r = 1;
        if (!ms_contains(&c->watch, t)) r = ms_append(&c->watch, t);
        TUBE_ASSIGN(t, NULL);
        if (!r) return reply_serr(c, MSG_OUT_OF_MEMORY);

        reply_line(c, STATE_SENDWORD, "WATCHING %d\r\n", c->watch.used);
        break;
    case OP_IGNORE:
        name = c->cmd + CMD_IGNORE_LEN;
        if (!name_is_ok(name, 200)) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        t = NULL;
        for (i = 0; i < c->watch.used; i++) {
            t = c->watch.items[i];
            if (strncmp(t->name, name, MAX_TUBE_NAME_LEN) == 0) break;
            t = NULL;
        }

        if (t && c->watch.used < 2) return reply_msg(c, MSG_NOT_IGNORED);

        if (t) ms_remove(&c->watch, t); /* may free t if refcount => 0 */
        t = NULL;

        reply_line(c, STATE_SENDWORD, "WATCHING %d\r\n", c->watch.used);
        break;
    default:
        return reply_msg(c, MSG_UNKNOWN_COMMAND);
    }
}

/* There are three reasons this function may be called. We need to check for
 * all of them.
 *
 *  1. A reserved job has run out of time.
 *  2. A waiting client's reserved job has entered the safety margin.
 *  3. A waiting client's requested timeout has occurred.
 *
 * If any of these happen, we must do the appropriate thing. */
static void
h_conn_timeout(conn c)
{
    int r, should_timeout = 0;
    job j;

    /* Check if the client was trying to reserve a job. */
    if (conn_waiting(c) && conn_has_close_deadline(c)) should_timeout = 1;

    /* Check if any reserved jobs have run out of time. We should do this
     * whether or not the client is waiting for a new reservation. */
    while ((j = soonest_job(c))) {
        if (j->deadline > time(NULL)) break;
        timeout_ct++; /* stats */
        j->timeout_ct++;
        r = enqueue_job(remove_this_reserved_job(c, j), 0);
        if (!r) bury_job(j); /* there was no room in the queue, so bury it */
        r = conn_update_evq(c, c->evq.ev_events);
        if (r == -1) return twarnx("conn_update_evq() failed"), conn_close(c);
    }

    if (should_timeout) {
        dprintf("conn_waiting(%p) = %d\n", c, conn_waiting(c));
        return reply_msg(remove_waiting_conn(c), MSG_DEADLINE_SOON);
    } else if (conn_waiting(c) && c->pending_timeout >= 0) {
        dprintf("conn_waiting(%p) = %d\n", c, conn_waiting(c));
        c->pending_timeout=-1;
        return reply_msg(remove_waiting_conn(c), MSG_TIMED_OUT);
    }
}

void
enter_drain_mode(int sig)
{
    drain_mode = 1;
}

static void
do_cmd(conn c)
{
    dispatch_cmd(c);
    fill_extra_data(c);
}

static void
reset_conn(conn c)
{
    int r;

    r = conn_update_evq(c, EV_READ | EV_PERSIST);
    if (r == -1) return twarnx("update events failed"), conn_close(c);

    /* was this a peek or stats command? */
    if (!has_reserved_this_job(c, c->out_job)) job_free(c->out_job);
    c->out_job = NULL;

    c->reply_sent = 0; /* now that we're done, reset this */
    c->state = STATE_WANTCOMMAND;
}

static void
h_conn_data(conn c)
{
    int r, to_read;
    job j;
    struct iovec iov[2];

    switch (c->state) {
    case STATE_WANTCOMMAND:
        r = read(c->fd, c->cmd + c->cmd_read, LINE_BUF_SIZE - c->cmd_read);
        if (r == -1) return check_err(c, "read()");
        if (r == 0) return conn_close(c); /* the client hung up */

        c->cmd_read += r; /* we got some bytes */

        c->cmd_len = cmd_len(c); /* find the EOL */

        /* yay, complete command line */
        if (c->cmd_len) return do_cmd(c);

        /* c->cmd_read > LINE_BUF_SIZE can't happen */

        /* command line too long? */
        if (c->cmd_read == LINE_BUF_SIZE) {
            c->cmd_read = 0; /* discard the input so far */
            return reply_msg(c, MSG_BAD_FORMAT);
        }

        /* otherwise we have an incomplete line, so just keep waiting */
        break;
    case STATE_BITBUCKET:
        /* Invert the meaning of in_job_read while throwing away data -- it
         * counts the bytes that remain to be thrown away. */
        to_read = min(c->in_job_read, BUCKET_BUF_SIZE);
        r = read(c->fd, bucket, to_read);
        if (r == -1) return check_err(c, "read()");
        if (r == 0) return conn_close(c); /* the client hung up */

        c->in_job_read -= r; /* we got some bytes */

        /* (c->in_job_read < 0) can't happen */

        if (c->in_job_read == 0) return reply_serr(c, MSG_OUT_OF_MEMORY);
        break;
    case STATE_WANTDATA:
        j = c->in_job;

        r = read(c->fd, j->body + c->in_job_read, j->body_size -c->in_job_read);
        if (r == -1) return check_err(c, "read()");
        if (r == 0) return conn_close(c); /* the client hung up */

        c->in_job_read += r; /* we got some bytes */

        /* (j->in_job_read > j->body_size) can't happen */

        maybe_enqueue_incoming_job(c);
        break;
    case STATE_SENDWORD:
        r= write(c->fd, c->reply + c->reply_sent, c->reply_len - c->reply_sent);
        if (r == -1) return check_err(c, "write()");
        if (r == 0) return conn_close(c); /* the client hung up */

        c->reply_sent += r; /* we got some bytes */

        /* (c->reply_sent > c->reply_len) can't happen */

        if (c->reply_sent == c->reply_len) return reset_conn(c);

        /* otherwise we sent an incomplete reply, so just keep waiting */
        break;
    case STATE_SENDJOB:
        j = c->out_job;

        iov[0].iov_base = (void *)(c->reply + c->reply_sent);
        iov[0].iov_len = c->reply_len - c->reply_sent; /* maybe 0 */
        iov[1].iov_base = j->body + c->out_job_sent;
        iov[1].iov_len = j->body_size - c->out_job_sent;

        r = writev(c->fd, iov, 2);
        if (r == -1) return check_err(c, "writev()");
        if (r == 0) return conn_close(c); /* the client hung up */

        /* update the sent values */
        c->reply_sent += r;
        if (c->reply_sent >= c->reply_len) {
            c->out_job_sent += c->reply_sent - c->reply_len;
            c->reply_sent = c->reply_len;
        }

        /* (c->out_job_sent > j->body_size) can't happen */

        /* are we done? */
        if (c->out_job_sent == j->body_size) return reset_conn(c);

        /* otherwise we sent incomplete data, so just keep waiting */
        break;
    case STATE_WAIT: /* keep an eye out in case they hang up */
        /* but don't hang up just because our buffer is full */
        if (LINE_BUF_SIZE - c->cmd_read < 1) break;

        r = read(c->fd, c->cmd + c->cmd_read, LINE_BUF_SIZE - c->cmd_read);
        if (r == -1) return check_err(c, "read()");
        if (r == 0) return conn_close(c); /* the client hung up */
        c->cmd_read += r; /* we got some bytes */
    }
}

#define want_command(c) ((c)->fd && ((c)->state == STATE_WANTCOMMAND))
#define cmd_data_ready(c) (want_command(c) && (c)->cmd_read)

static void
h_conn(const int fd, const short which, conn c)
{
    if (fd != c->fd) {
        twarnx("Argh! event fd doesn't match conn fd.");
        close(fd);
        return conn_close(c);
    }

    switch (which) {
    case EV_TIMEOUT:
        h_conn_timeout(c);
        event_add(&c->evq, NULL); /* seems to be necessary */
        break;
    case EV_READ:
        /* fall through... */
    case EV_WRITE:
        /* fall through... */
    default:
        h_conn_data(c);
    }

    while (cmd_data_ready(c) && (c->cmd_len = cmd_len(c))) do_cmd(c);
}

static void
h_delay()
{
    int r;
    job j;
    time_t t;

    t = time(NULL);
    while ((j = delay_q_peek())) {
        if (j->deadline > t) break;
        j = delay_q_take();
        r = enqueue_job(j, 0);
        if (!r) bury_job(j); /* there was no room in the queue, so bury it */
    }

    set_main_delay_timeout();
}

void
h_accept(const int fd, const short which, struct event *ev)
{
    conn c;
    int cfd, flags, r;
    socklen_t addrlen;
    struct sockaddr addr;

    if (which == EV_TIMEOUT) return h_delay();

    addrlen = sizeof addr;
    cfd = accept(fd, &addr, &addrlen);
    if (cfd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) twarn("accept()");
        if (errno == EMFILE) brake();
        return;
    }

    flags = fcntl(cfd, F_GETFL, 0);
    if (flags < 0) return twarn("getting flags"), close(cfd), v();

    r = fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
    if (r < 0) return twarn("setting O_NONBLOCK"), close(cfd), v();

    c = make_conn(cfd, STATE_WANTCOMMAND, default_tube, default_tube);
    if (!c) return twarnx("make_conn() failed"), close(cfd), brake();

    dprintf("accepted conn, fd=%d\n", cfd);
    r = conn_set_evq(c, EV_READ | EV_PERSIST, (evh) h_conn);
    if (r == -1) return twarnx("conn_set_evq() failed"), close(cfd), brake();
}

void
prot_init()
{
    start_time = time(NULL);
    memset(op_ct, 0, sizeof(op_ct));

    ms_init(&tubes, NULL, NULL);

    TUBE_ASSIGN(default_tube, tube_find_or_make("default"));
    if (!default_tube) twarnx("Out of memory during startup!");
}

void
prot_replay_binlog()
{
    struct job binlog_jobs;
    job j, nj;
    unsigned int delay;

    binlog_jobs.prev = binlog_jobs.next = &binlog_jobs;
    binlog_read(&binlog_jobs);

    for(j = binlog_jobs.next ; j != &binlog_jobs ; j = nj) {
        nj = j->next;
        job_remove(j);
        delay = 0;
        switch (j->state) {
        case JOB_STATE_BURIED:
            bury_job(j);
            break;
        case JOB_STATE_DELAYED:
            if (start_time < j->deadline) delay = j->deadline - start_time;
        default:
            enqueue_job(j,delay);
    }
  }
}
