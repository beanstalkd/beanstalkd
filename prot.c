#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include "dat.h"

/* job body cannot be greater than this many bytes long */
size_t job_data_size_limit = JOB_DATA_SIZE_LIMIT_DEFAULT;

#define NAME_CHARS \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefghijklmnopqrstuvwxyz" \
    "0123456789-+/;.$_()"

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
#define CMD_TOUCH "touch "
#define CMD_STATS "stats"
#define CMD_JOBSTATS "stats-job "
#define CMD_USE "use "
#define CMD_WATCH "watch "
#define CMD_IGNORE "ignore "
#define CMD_LIST_TUBES "list-tubes"
#define CMD_LIST_TUBE_USED "list-tube-used"
#define CMD_LIST_TUBES_WATCHED "list-tubes-watched"
#define CMD_STATS_TUBE "stats-tube "
#define CMD_QUIT "quit"
#define CMD_PAUSE_TUBE "pause-tube"

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
#define CMD_TOUCH_LEN CONSTSTRLEN(CMD_TOUCH)
#define CMD_STATS_LEN CONSTSTRLEN(CMD_STATS)
#define CMD_JOBSTATS_LEN CONSTSTRLEN(CMD_JOBSTATS)
#define CMD_USE_LEN CONSTSTRLEN(CMD_USE)
#define CMD_WATCH_LEN CONSTSTRLEN(CMD_WATCH)
#define CMD_IGNORE_LEN CONSTSTRLEN(CMD_IGNORE)
#define CMD_LIST_TUBES_LEN CONSTSTRLEN(CMD_LIST_TUBES)
#define CMD_LIST_TUBE_USED_LEN CONSTSTRLEN(CMD_LIST_TUBE_USED)
#define CMD_LIST_TUBES_WATCHED_LEN CONSTSTRLEN(CMD_LIST_TUBES_WATCHED)
#define CMD_STATS_TUBE_LEN CONSTSTRLEN(CMD_STATS_TUBE)
#define CMD_PAUSE_TUBE_LEN CONSTSTRLEN(CMD_PAUSE_TUBE)

#define MSG_FOUND "FOUND"
#define MSG_NOTFOUND "NOT_FOUND\r\n"
#define MSG_RESERVED "RESERVED"
#define MSG_DEADLINE_SOON "DEADLINE_SOON\r\n"
#define MSG_TIMED_OUT "TIMED_OUT\r\n"
#define MSG_DELETED "DELETED\r\n"
#define MSG_RELEASED "RELEASED\r\n"
#define MSG_BURIED "BURIED\r\n"
#define MSG_TOUCHED "TOUCHED\r\n"
#define MSG_BURIED_FMT "BURIED %"PRIu64"\r\n"
#define MSG_INSERTED_FMT "INSERTED %"PRIu64"\r\n"
#define MSG_NOT_IGNORED "NOT_IGNORED\r\n"

#define MSG_NOTFOUND_LEN CONSTSTRLEN(MSG_NOTFOUND)
#define MSG_DELETED_LEN CONSTSTRLEN(MSG_DELETED)
#define MSG_TOUCHED_LEN CONSTSTRLEN(MSG_TOUCHED)
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
#define OP_TOUCH 21
#define OP_QUIT 22
#define OP_PAUSE_TUBE 23
#define TOTAL_OPS 24

#define STATS_FMT "---\n" \
    "current-jobs-urgent: %u\n" \
    "current-jobs-ready: %u\n" \
    "current-jobs-reserved: %u\n" \
    "current-jobs-delayed: %u\n" \
    "current-jobs-buried: %u\n" \
    "cmd-put: %" PRIu64 "\n" \
    "cmd-peek: %" PRIu64 "\n" \
    "cmd-peek-ready: %" PRIu64 "\n" \
    "cmd-peek-delayed: %" PRIu64 "\n" \
    "cmd-peek-buried: %" PRIu64 "\n" \
    "cmd-reserve: %" PRIu64 "\n" \
    "cmd-reserve-with-timeout: %" PRIu64 "\n" \
    "cmd-delete: %" PRIu64 "\n" \
    "cmd-release: %" PRIu64 "\n" \
    "cmd-use: %" PRIu64 "\n" \
    "cmd-watch: %" PRIu64 "\n" \
    "cmd-ignore: %" PRIu64 "\n" \
    "cmd-bury: %" PRIu64 "\n" \
    "cmd-kick: %" PRIu64 "\n" \
    "cmd-touch: %" PRIu64 "\n" \
    "cmd-stats: %" PRIu64 "\n" \
    "cmd-stats-job: %" PRIu64 "\n" \
    "cmd-stats-tube: %" PRIu64 "\n" \
    "cmd-list-tubes: %" PRIu64 "\n" \
    "cmd-list-tube-used: %" PRIu64 "\n" \
    "cmd-list-tubes-watched: %" PRIu64 "\n" \
    "cmd-pause-tube: %" PRIu64 "\n" \
    "job-timeouts: %" PRIu64 "\n" \
    "total-jobs: %" PRIu64 "\n" \
    "max-job-size: %zu\n" \
    "current-tubes: %zu\n" \
    "current-connections: %u\n" \
    "current-producers: %u\n" \
    "current-workers: %u\n" \
    "current-waiting: %u\n" \
    "total-connections: %u\n" \
    "pid: %ld\n" \
    "version: %s\n" \
    "rusage-utime: %d.%06d\n" \
    "rusage-stime: %d.%06d\n" \
    "uptime: %u\n" \
    "binlog-oldest-index: %d\n" \
    "binlog-current-index: %d\n" \
    "binlog-records-migrated: %" PRId64 "\n" \
    "binlog-records-written: %" PRId64 "\n" \
    "binlog-max-size: %d\n" \
    "\r\n"

#define STATS_TUBE_FMT "---\n" \
    "name: %s\n" \
    "current-jobs-urgent: %u\n" \
    "current-jobs-ready: %u\n" \
    "current-jobs-reserved: %u\n" \
    "current-jobs-delayed: %u\n" \
    "current-jobs-buried: %u\n" \
    "total-jobs: %" PRIu64 "\n" \
    "current-using: %u\n" \
    "current-watching: %u\n" \
    "current-waiting: %u\n" \
    "cmd-delete: %" PRIu64 "\n" \
    "cmd-pause-tube: %u\n" \
    "pause: %" PRIu64 "\n" \
    "pause-time-left: %" PRId64 "\n" \
    "\r\n"

#define STATS_JOB_FMT "---\n" \
    "id: %" PRIu64 "\n" \
    "tube: %s\n" \
    "state: %s\n" \
    "pri: %u\n" \
    "age: %" PRId64 "\n" \
    "delay: %" PRId64 "\n" \
    "ttr: %" PRId64 "\n" \
    "time-left: %" PRId64 "\n" \
    "file: %d\n" \
    "reserves: %u\n" \
    "timeouts: %u\n" \
    "releases: %u\n" \
    "buries: %u\n" \
    "kicks: %u\n" \
    "\r\n"

/* this number is pretty arbitrary */
#define BUCKET_BUF_SIZE 1024

static char bucket[BUCKET_BUF_SIZE];

static uint ready_ct = 0;
static struct stats global_stat = {0, 0, 0, 0, 0};

static tube default_tube;

static int drain_mode = 0;
static int64 started_at;
static uint64 op_ct[TOTAL_OPS], timeout_ct = 0;

static struct conn dirty = {&dirty, &dirty};

/* Doubly-linked list of connections with at least one reserved job. */
static struct conn running = { &running, &running };

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
    CMD_RESERVE_TIMEOUT,
    CMD_TOUCH,
    CMD_QUIT,
    CMD_PAUSE_TUBE
};

static job remove_buried_job(job j);

static int
buried_job_p(tube t)
{
    return job_list_any_p(&t->buried);
}

static void
reply(conn c, const char *line, int len, int state)
{
    if (!c) return;

    connwant(c, 'w', &dirty);
    c->reply = line;
    c->reply_len = len;
    c->reply_sent = 0;
    c->state = state;
    if (verbose >= 2) {
        printf(">%d reply %.*s\n", c->sock.fd, len-2, line);
    }
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

    return reply_line(c, STATE_SENDJOB, "%s %"PRIu64" %u\r\n",
                      word, j->r.id, j->r.body_size - 2);
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
    j->r.deadline_at = nanoseconds() + j->r.ttr;
    global_stat.reserved_ct++; /* stats */
    j->tube->stat.reserved_ct++;
    j->r.reserve_ct++;
    j->r.state = Reserved;
    job_insert(&c->reserved_jobs, j);
    j->reserver = c;
    if (c->soonest_job && j->r.deadline_at < c->soonest_job->r.deadline_at) {
        c->soonest_job = j;
    }
    return reply_job(c, j, MSG_RESERVED);
}

static job
next_eligible_job(int64 now)
{
    tube t;
    size_t i;
    job j = NULL, candidate;

    for (i = 0; i < tubes.used; i++) {
        t = tubes.items[i];
        if (t->pause) {
            if (t->deadline_at > now) continue;
            t->pause = 0;
        }
        if (t->waiting.used && t->ready.len) {
            candidate = t->ready.data[0];
            if (!j || job_pri_less(candidate, j)) {
                j = candidate;
            }
        }
    }

    return j;
}

static void
process_queue()
{
    job j;
    int64 now = nanoseconds();

    while ((j = next_eligible_job(now))) {
        heapremove(&j->tube->ready, j->heap_index);
        ready_ct--;
        if (j->r.pri < URGENT_THRESHOLD) {
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
        if (t->delay.len == 0) {
            continue;
        }
        nj = t->delay.data[0];
        if (!j || nj->r.deadline_at < j->r.deadline_at) j = nj;
    }

    return j;
}

static int
enqueue_job(Server *s, job j, int64 delay, char update_store)
{
    int r;

    j->reserver = NULL;
    if (delay) {
        j->r.deadline_at = nanoseconds() + delay;
        r = heapinsert(&j->tube->delay, j);
        if (!r) return 0;
        j->r.state = Delayed;
    } else {
        r = heapinsert(&j->tube->ready, j);
        if (!r) return 0;
        j->r.state = Ready;
        ready_ct++;
        if (j->r.pri < URGENT_THRESHOLD) {
            global_stat.urgent_ct++;
            j->tube->stat.urgent_ct++;
        }
    }

    if (update_store) {
        if (!walwrite(&s->wal, j)) {
            return 0;
        }
        walmaint(&s->wal);
    }

    process_queue();
    return 1;
}

static int
bury_job(Server *s, job j, char update_store)
{
    int z;

    if (update_store) {
        z = walresvupdate(&s->wal, j);
        if (!z) return 0;
        j->walresv += z;
    }

    job_insert(&j->tube->buried, j);
    global_stat.buried_ct++;
    j->tube->stat.buried_ct++;
    j->r.state = Buried;
    j->reserver = NULL;
    j->r.bury_ct++;

    if (update_store) {
        if (!walwrite(&s->wal, j)) {
            return 0;
        }
        walmaint(&s->wal);
    }

    return 1;
}

void
enqueue_reserved_jobs(conn c)
{
    int r;
    job j;

    while (job_list_any_p(&c->reserved_jobs)) {
        j = job_remove(c->reserved_jobs.next);
        r = enqueue_job(c->srv, j, 0, 0);
        if (r < 1) bury_job(c->srv, j, 0);
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
    if (!j) {
        return 0;
    }
    heapremove(&j->tube->delay, j->heap_index);
    return j;
}

static int
kick_buried_job(Server *s, tube t)
{
    int r;
    job j;
    int z;

    if (!buried_job_p(t)) return 0;
    j = remove_buried_job(t->buried.next);

    z = walresvupdate(&s->wal, j);
    if (!z) return heapinsert(&t->delay, j), 0; /* put it back */
    j->walresv += z;

    j->r.kick_ct++;
    r = enqueue_job(s, j, 0, 1);
    if (r == 1) return 1;

    /* ready queue is full, so bury it */
    bury_job(s, j, 0);
    return 0;
}

static uint
get_delayed_job_ct()
{
    tube t;
    size_t i;
    uint count = 0;

    for (i = 0; i < tubes.used; i++) {
        t = tubes.items[i];
        count += t->delay.len;
    }
    return count;
}

static int
kick_delayed_job(Server *s, tube t)
{
    int r;
    job j;
    int z;

    if (t->delay.len == 0) {
        return 0;
    }

    j = heapremove(&t->delay, 0);

    z = walresvupdate(&s->wal, j);
    if (!z) return heapinsert(&t->delay, j), 0; /* put it back */
    j->walresv += z;

    j->r.kick_ct++;
    r = enqueue_job(s, j, 0, 1);
    if (r == 1) return 1;

    /* ready queue is full, so delay it again */
    r = enqueue_job(s, j, j->r.delay, 0);
    if (r == 1) return 0;

    /* last resort */
    bury_job(s, j, 0);
    return 0;
}

/* return the number of jobs successfully kicked */
static uint
kick_buried_jobs(Server *s, tube t, uint n)
{
    uint i;
    for (i = 0; (i < n) && kick_buried_job(s, t); ++i);
    return i;
}

/* return the number of jobs successfully kicked */
static uint
kick_delayed_jobs(Server *s, tube t, uint n)
{
    uint i;
    for (i = 0; (i < n) && kick_delayed_job(s, t); ++i);
    return i;
}

static uint
kick_jobs(Server *s, tube t, uint n)
{
    if (buried_job_p(t)) return kick_buried_jobs(s, t, n);
    return kick_delayed_jobs(s, t, n);
}

static job
remove_buried_job(job j)
{
    if (!j || j->r.state != Buried) return NULL;
    j = job_remove(j);
    if (j) {
        global_stat.buried_ct--;
        j->tube->stat.buried_ct--;
    }
    return j;
}

static job
remove_delayed_job(job j)
{
    if (!j || j->r.state != Delayed) return NULL;
    heapremove(&j->tube->delay, j->heap_index);

    return j;
}

static job
remove_ready_job(job j)
{
    if (!j || j->r.state != Ready) return NULL;
    heapremove(&j->tube->ready, j->heap_index);
    ready_ct--;
    if (j->r.pri < URGENT_THRESHOLD) {
        global_stat.urgent_ct--;
        j->tube->stat.urgent_ct--;
    }
    return j;
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
find_reserved_job_in_conn(conn c, job j)
{
    return (j && j->reserver == c && j->r.state == Reserved) ? j : NULL;
}

static job
touch_job(conn c, job j)
{
    j = find_reserved_job_in_conn(c, j);
    if (j) {
        j->r.deadline_at = nanoseconds() + j->r.ttr;
        c->soonest_job = NULL;
    }
    return j;
}

static job
peek_job(uint64 id)
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
    TEST_CMD(c->cmd, CMD_TOUCH, OP_TOUCH);
    TEST_CMD(c->cmd, CMD_JOBSTATS, OP_JOBSTATS);
    TEST_CMD(c->cmd, CMD_STATS_TUBE, OP_STATS_TUBE);
    TEST_CMD(c->cmd, CMD_STATS, OP_STATS);
    TEST_CMD(c->cmd, CMD_USE, OP_USE);
    TEST_CMD(c->cmd, CMD_WATCH, OP_WATCH);
    TEST_CMD(c->cmd, CMD_IGNORE, OP_IGNORE);
    TEST_CMD(c->cmd, CMD_LIST_TUBES_WATCHED, OP_LIST_TUBES_WATCHED);
    TEST_CMD(c->cmd, CMD_LIST_TUBE_USED, OP_LIST_TUBE_USED);
    TEST_CMD(c->cmd, CMD_LIST_TUBES, OP_LIST_TUBES);
    TEST_CMD(c->cmd, CMD_QUIT, OP_QUIT);
    TEST_CMD(c->cmd, CMD_PAUSE_TUBE, OP_PAUSE_TUBE);
    return OP_UNKNOWN;
}

/* Copy up to body_size trailing bytes into the job, then the rest into the cmd
 * buffer. If c->in_job exists, this assumes that c->in_job->body is empty.
 * This function is idempotent(). */
static void
fill_extra_data(conn c)
{
    int extra_bytes, job_data_bytes = 0, cmd_bytes;

    if (!c->sock.fd) return; /* the connection was closed */
    if (!c->cmd_len) return; /* we don't have a complete command */

    /* how many extra bytes did we read? */
    extra_bytes = c->cmd_read - c->cmd_len;

    /* how many bytes should we put into the job body? */
    if (c->in_job) {
        job_data_bytes = min(extra_bytes, c->in_job->r.body_size);
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
_skip(conn c, int n, const char *line, int len)
{
    /* Invert the meaning of in_job_read while throwing away data -- it
     * counts the bytes that remain to be thrown away. */
    c->in_job = 0;
    c->in_job_read = n;
    fill_extra_data(c);

    if (c->in_job_read == 0) return reply(c, line, len, STATE_SENDWORD);

    c->reply = line;
    c->reply_len = len;
    c->reply_sent = 0;
    c->state = STATE_BITBUCKET;
    return;
}

#define skip(c,n,m) (_skip(c,n,m,CONSTSTRLEN(m)))

static void
enqueue_incoming_job(conn c)
{
    int r;
    job j = c->in_job;

    c->in_job = NULL; /* the connection no longer owns this job */
    c->in_job_read = 0;

    /* check if the trailer is present and correct */
    if (memcmp(j->body + j->r.body_size - 2, "\r\n", 2)) {
        job_free(j);
        return reply_msg(c, MSG_EXPECTED_CRLF);
    }

    if (verbose >= 2) {
        printf("<%d job %"PRIu64"\n", c->sock.fd, j->r.id);
    }

    if (drain_mode) {
        job_free(j);
        return reply_serr(c, MSG_DRAINING);
    }

    if (j->walresv) return reply_serr(c, MSG_INTERNAL_ERROR);
    j->walresv = walresvput(&c->srv->wal, j);
    if (!j->walresv) return reply_serr(c, MSG_OUT_OF_MEMORY);

    /* we have a complete job, so let's stick it in the pqueue */
    r = enqueue_job(c->srv, j, j->r.delay, 1);
    if (r < 0) return reply_serr(c, MSG_INTERNAL_ERROR);

    op_ct[OP_PUT]++; /* stats */
    global_stat.total_jobs_ct++;
    j->tube->stat.total_jobs_ct++;

    if (r == 1) return reply_line(c, STATE_SENDWORD, MSG_INSERTED_FMT, j->r.id);

    /* out of memory trying to grow the queue, so it gets buried */
    bury_job(c->srv, j, 0);
    reply_line(c, STATE_SENDWORD, MSG_BURIED_FMT, j->r.id);
}

static uint
uptime()
{
    return (nanoseconds() - started_at) / 1000000000;
}

static int
fmt_stats(char *buf, size_t size, void *x)
{
    int whead = 0, wcur = 0;
    Server *srv;
    struct rusage ru = {{0, 0}, {0, 0}};

    srv = x;

    if (srv->wal.head) {
        whead = srv->wal.head->seq;
    }

    if (srv->wal.cur) {
        wcur = srv->wal.cur->seq;
    }

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
            op_ct[OP_TOUCH],
            op_ct[OP_STATS],
            op_ct[OP_JOBSTATS],
            op_ct[OP_STATS_TUBE],
            op_ct[OP_LIST_TUBES],
            op_ct[OP_LIST_TUBE_USED],
            op_ct[OP_LIST_TUBES_WATCHED],
            op_ct[OP_PAUSE_TUBE],
            timeout_ct,
            global_stat.total_jobs_ct,
            job_data_size_limit,
            tubes.used,
            count_cur_conns(),
            count_cur_producers(),
            count_cur_workers(),
            global_stat.waiting_ct,
            count_tot_conns(),
            (long) getpid(),
            version,
            (int) ru.ru_utime.tv_sec, (int) ru.ru_utime.tv_usec,
            (int) ru.ru_stime.tv_sec, (int) ru.ru_stime.tv_usec,
            uptime(),
            whead,
            wcur,
            srv->wal.nmig,
            srv->wal.nrec,
            srv->wal.filesz);

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
read_pri(uint *pri, const char *buf, char **end)
{
    char *tend;
    uint tpri;

    errno = 0;
    while (buf[0] == ' ') buf++;
    if (!isdigit(buf[0])) return -1;
    tpri = strtoul(buf, &tend, 10);
    if (tend == buf) return -1;
    if (errno && errno != ERANGE) return -1;
    if (!end && tend[0] != '\0') return -1;

    if (pri) *pri = tpri;
    if (end) *end = tend;
    return 0;
}

/* Read a delay value from the given buffer and place it in delay.
 * The interface and behavior are analogous to read_pri(). */
static int
read_delay(int64 *delay, const char *buf, char **end)
{
    int r;
    uint delay_sec;

    r = read_pri(&delay_sec, buf, end);
    if (r) return r;
    *delay = ((int64) delay_sec) * 1000000000;
    return 0;
}

/* Read a timeout value from the given buffer and place it in ttr.
 * The interface and behavior are the same as in read_delay(). */
static int
read_ttr(int64 *ttr, const char *buf, char **end)
{
    return read_delay(ttr, buf, end);
}

/* Read a tube name from the given buffer moving the buffer to the name start */
static int
read_tube_name(char **tubename, char *buf, char **end)
{
    size_t len;

    while (buf[0] == ' ') buf++;
    len = strspn(buf, NAME_CHARS);
    if (len == 0) return -1;
    if (tubename) *tubename = buf;
    if (end) *end = buf + len;
    return 0;
}

static void
wait_for_job(conn c, int timeout)
{
    c->state = STATE_WAIT;
    enqueue_waiting_conn(c);

    /* Set the pending timeout to the requested timeout amount */
    c->pending_timeout = timeout;

    connwant(c, 'h', &dirty); // only care if they hang up
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

    /* Mark this job as a copy so it can be appropriately freed later on */
    c->out_job->r.state = Copy;

    /* now actually format the stats data */
    r = fmt(c->out_job->body, stats_len, data);
    /* and set the actual body size */
    c->out_job->r.body_size = r;
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

    /* Mark this job as a copy so it can be appropriately freed later on */
    c->out_job->r.state = Copy;

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
    int64 t;
    int64 time_left;
    int file = 0;

    t = nanoseconds();
    if (j->r.state == Reserved || j->r.state == Delayed) {
        time_left = (j->r.deadline_at - t) / 1000000000;
    } else {
        time_left = 0;
    }
    if (j->file) {
        file = j->file->seq;
    }
    return snprintf(buf, size, STATS_JOB_FMT,
            j->r.id,
            j->tube->name,
            job_state(j),
            j->r.pri,
            (t - j->r.created_at) / 1000000000,
            j->r.delay / 1000000000,
            j->r.ttr / 1000000000,
            time_left,
            file,
            j->r.reserve_ct,
            j->r.timeout_ct,
            j->r.release_ct,
            j->r.bury_ct,
            j->r.kick_ct);
}

static int
fmt_stats_tube(char *buf, size_t size, tube t)
{
    uint64 time_left;

    if (t->pause > 0) {
        time_left = (t->deadline_at - nanoseconds()) / 1000000000;
    } else {
        time_left = 0;
    }
    return snprintf(buf, size, STATS_TUBE_FMT,
            t->name,
            t->stat.urgent_ct,
            t->ready.len,
            t->stat.reserved_ct,
            t->delay.len,
            t->stat.buried_ct,
            t->stat.total_jobs_ct,
            t->using_ct,
            t->watching_ct,
            t->stat.waiting_ct,
            t->stat.total_delete_ct,
            t->stat.pause_ct,
            t->pause / 1000000000,
            time_left);
}

static void
maybe_enqueue_incoming_job(conn c)
{
    job j = c->in_job;

    /* do we have a complete job? */
    if (c->in_job_read == j->r.body_size) return enqueue_incoming_job(c);

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
        j->reserver = NULL;
    }
    c->soonest_job = NULL;
    if (!job_list_any_p(&c->reserved_jobs)) conn_remove(c);
    return j;
}

static job
remove_reserved_job(conn c, job j)
{
    return remove_this_reserved_job(c, find_reserved_job_in_conn(c, j));
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
    int z;
    uint count;
    job j = 0;
    byte type;
    char *size_buf, *delay_buf, *ttr_buf, *pri_buf, *end_buf, *name;
    uint pri, body_size;
    int64 delay, ttr;
    uint64 id;
    tube t = NULL;

    /* NUL-terminate this string so we can use strtol and friends */
    c->cmd[c->cmd_len - 2] = '\0';

    /* check for possible maliciousness */
    if (strlen(c->cmd) != c->cmd_len - 2) {
        return reply_msg(c, MSG_BAD_FORMAT);
    }

    type = which_cmd(c);
    if (verbose >= 2) {
        printf("<%d command %s\n", c->sock.fd, op_names[type]);
    }

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
            /* throw away the job body and respond with JOB_TOO_BIG */
            return skip(c, body_size + 2, MSG_JOB_TOO_BIG);
        }

        /* don't allow trailing garbage */
        if (end_buf[0] != '\0') return reply_msg(c, MSG_BAD_FORMAT);

        conn_set_producer(c);

        if (ttr < 1000000000) {
            ttr = 1000000000;
        }

        c->in_job = make_job(pri, delay, ttr, body_size + 2, c->use);

        /* OOM? */
        if (!c->in_job) {
            /* throw away the job body and respond with OUT_OF_MEMORY */
            twarnx("server error: " MSG_OUT_OF_MEMORY);
            return skip(c, body_size + 2, MSG_OUT_OF_MEMORY);
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

        if (c->use->ready.len) {
            j = job_copy(c->use->ready.data[0]);
        }

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        reply_job(c, j, MSG_FOUND);
        break;
    case OP_PEEK_DELAYED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_PEEK_DELAYED_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }
        op_ct[type]++;

        if (c->use->delay.len) {
            j = job_copy(c->use->delay.data[0]);
        }

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

        j = job_find(id);
        j = remove_reserved_job(c, j) ? :
            remove_ready_job(j) ? :
            remove_buried_job(j) ? :
            remove_delayed_job(j);

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        j->tube->stat.total_delete_ct++;

        j->r.state = Invalid;
        r = walwrite(&c->srv->wal, j);
        walmaint(&c->srv->wal);
        job_free(j);

        if (!r) return reply_serr(c, MSG_INTERNAL_ERROR);

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

        j = remove_reserved_job(c, job_find(id));

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        /* We want to update the delay deadline on disk, so reserve space for
         * that. */
        if (delay) {
            z = walresvupdate(&c->srv->wal, j);
            if (!z) return reply_serr(c, MSG_OUT_OF_MEMORY);
            j->walresv += z;
        }

        j->r.pri = pri;
        j->r.delay = delay;
        j->r.release_ct++;

        r = enqueue_job(c->srv, j, delay, !!delay);
        if (r < 0) return reply_serr(c, MSG_INTERNAL_ERROR);
        if (r == 1) {
            return reply(c, MSG_RELEASED, MSG_RELEASED_LEN, STATE_SENDWORD);
        }

        /* out of memory trying to grow the queue, so it gets buried */
        bury_job(c->srv, j, 0);
        reply(c, MSG_BURIED, MSG_BURIED_LEN, STATE_SENDWORD);
        break;
    case OP_BURY:
        errno = 0;
        id = strtoull(c->cmd + CMD_BURY_LEN, &pri_buf, 10);
        if (errno) return reply_msg(c, MSG_BAD_FORMAT);

        r = read_pri(&pri, pri_buf, NULL);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);
        op_ct[type]++;

        j = remove_reserved_job(c, job_find(id));

        if (!j) return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);

        j->r.pri = pri;
        r = bury_job(c->srv, j, 1);
        if (!r) return reply_serr(c, MSG_INTERNAL_ERROR);
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

        i = kick_jobs(c->srv, c->use, count);

        return reply_line(c, STATE_SENDWORD, "KICKED %u\r\n", i);
    case OP_TOUCH:
        errno = 0;
        id = strtoull(c->cmd + CMD_TOUCH_LEN, &end_buf, 10);
        if (errno) return twarn("strtoull"), reply_msg(c, MSG_BAD_FORMAT);

        op_ct[type]++;

        j = touch_job(c, job_find(id));

        if (j) {
            reply(c, MSG_TOUCHED, MSG_TOUCHED_LEN, STATE_SENDWORD);
        } else {
            return reply(c, MSG_NOTFOUND, MSG_NOTFOUND_LEN, STATE_SENDWORD);
        }
        break;
    case OP_STATS:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_STATS_LEN + 2) {
            return reply_msg(c, MSG_BAD_FORMAT);
        }

        op_ct[type]++;

        do_stats(c, fmt_stats, c->srv);
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
    case OP_QUIT:
        conn_close(c);
        break;
    case OP_PAUSE_TUBE:
        op_ct[type]++;

        r = read_tube_name(&name, c->cmd + CMD_PAUSE_TUBE_LEN, &delay_buf);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);

        r = read_delay(&delay, delay_buf, NULL);
        if (r) return reply_msg(c, MSG_BAD_FORMAT);

        *delay_buf = '\0';
        t = tube_find(name);
        if (!t) return reply_msg(c, MSG_NOTFOUND);

        t->deadline_at = nanoseconds() + delay;
        t->pause = delay;
        t->stat.pause_ct++;

        reply_line(c, STATE_SENDWORD, "PAUSED\r\n");
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
conn_timeout(conn c)
{
    int r, should_timeout = 0;
    job j;

    /* Check if the client was trying to reserve a job. */
    if (conn_waiting(c) && conn_has_close_deadline(c)) should_timeout = 1;

    /* Check if any reserved jobs have run out of time. We should do this
     * whether or not the client is waiting for a new reservation. */
    while ((j = soonest_job(c))) {
        if (j->r.deadline_at >= nanoseconds()) break;

        /* This job is in the middle of being written out. If we return it to
         * the ready queue, someone might free it before we finish writing it
         * out to the socket. So we'll copy it here and free the copy when it's
         * done sending. */
        if (j == c->out_job) {
            c->out_job = job_copy(c->out_job);
        }

        timeout_ct++; /* stats */
        j->r.timeout_ct++;
        r = enqueue_job(c->srv, remove_this_reserved_job(c, j), 0, 0);
        if (r < 1) bury_job(c->srv, j, 0); /* out of memory, so bury it */
        connsched(c);
    }

    if (should_timeout) {
        return reply_msg(remove_waiting_conn(c), MSG_DEADLINE_SOON);
    } else if (conn_waiting(c) && c->pending_timeout >= 0) {
        c->pending_timeout = -1;
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
    connwant(c, 'r', &dirty);

    /* was this a peek or stats command? */
    if (c->out_job && c->out_job->r.state == Copy) job_free(c->out_job);
    c->out_job = NULL;

    c->reply_sent = 0; /* now that we're done, reset this */
    c->state = STATE_WANTCOMMAND;
}

static void
conn_data(conn c)
{
    int r, to_read;
    job j;
    struct iovec iov[2];

    switch (c->state) {
    case STATE_WANTCOMMAND:
        r = read(c->sock.fd, c->cmd + c->cmd_read, LINE_BUF_SIZE - c->cmd_read);
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
        r = read(c->sock.fd, bucket, to_read);
        if (r == -1) return check_err(c, "read()");
        if (r == 0) return conn_close(c); /* the client hung up */

        c->in_job_read -= r; /* we got some bytes */

        /* (c->in_job_read < 0) can't happen */

        if (c->in_job_read == 0) {
            return reply(c, c->reply, c->reply_len, STATE_SENDWORD);
        }
        break;
    case STATE_WANTDATA:
        j = c->in_job;

        r = read(c->sock.fd, j->body + c->in_job_read, j->r.body_size -c->in_job_read);
        if (r == -1) return check_err(c, "read()");
        if (r == 0) return conn_close(c); /* the client hung up */

        c->in_job_read += r; /* we got some bytes */

        /* (j->in_job_read > j->r.body_size) can't happen */

        maybe_enqueue_incoming_job(c);
        break;
    case STATE_SENDWORD:
        r= write(c->sock.fd, c->reply + c->reply_sent, c->reply_len - c->reply_sent);
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
        iov[1].iov_len = j->r.body_size - c->out_job_sent;

        r = writev(c->sock.fd, iov, 2);
        if (r == -1) return check_err(c, "writev()");
        if (r == 0) return conn_close(c); /* the client hung up */

        /* update the sent values */
        c->reply_sent += r;
        if (c->reply_sent >= c->reply_len) {
            c->out_job_sent += c->reply_sent - c->reply_len;
            c->reply_sent = c->reply_len;
        }

        /* (c->out_job_sent > j->r.body_size) can't happen */

        /* are we done? */
        if (c->out_job_sent == j->r.body_size) {
            if (verbose >= 2) {
                printf(">%d job %"PRIu64"\n", c->sock.fd, j->r.id);
            }
            return reset_conn(c);
        }

        /* otherwise we sent incomplete data, so just keep waiting */
        break;
    case STATE_WAIT:
        // nothing
        break;
    }
}

#define want_command(c) ((c)->sock.fd && ((c)->state == STATE_WANTCOMMAND))
#define cmd_data_ready(c) (want_command(c) && (c)->cmd_read)

static void
update_conns()
{
    int r;
    conn c;

    while ((c = conn_remove(dirty.next))) { /* assignment */
        r = sockwant(&c->sock, c->rw);
        if (r == -1) {
            twarn("sockwant");
            conn_close(c);
        }
    }
}

static void
h_conn(const int fd, const short which, conn c)
{
    if (fd != c->sock.fd) {
        twarnx("Argh! event fd doesn't match conn fd.");
        close(fd);
        conn_close(c);
        update_conns();
        return;
    }

    if (which == 'h') {
        conn_close(c);
        return;
    }

    conn_data(c);
    while (cmd_data_ready(c) && (c->cmd_len = cmd_len(c))) do_cmd(c);
    update_conns();
}

static void
prothandle(conn c, int ev)
{
    h_conn(c->sock.fd, ev, c);
}

void
prottick(Server *s)
{
    int r;
    job j;
    int64 now;
    int i;
    tube t;

    now = nanoseconds();
    while ((j = delay_q_peek())) {
        if (j->r.deadline_at > now) break;
        j = delay_q_take();
        r = enqueue_job(s, j, 0, 0);
        if (r < 1) bury_job(s, j, 0); /* out of memory, so bury it */
    }

    for (i = 0; i < tubes.used; i++) {
        t = tubes.items[i];

        if (t->pause && t->deadline_at <= now) {
            t->pause = 0;
            process_queue();
        }
    }

    while (s->conns.len) {
        conn c = s->conns.data[0];
        if (c->tickat > now) {
            break;
        }

        heapremove(&s->conns, 0);
        conn_timeout(c);
    }

    update_conns();
}

void
h_accept(const int fd, const short which, Server *s)
{
    conn c;
    int cfd, flags, r;
    socklen_t addrlen;
    struct sockaddr_in6 addr;

    addrlen = sizeof addr;
    cfd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if (cfd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) twarn("accept()");
        update_conns();
        return;
    }
    if (verbose) {
        printf("accept %d\n", cfd);
    }

    flags = fcntl(cfd, F_GETFL, 0);
    if (flags < 0) {
        twarn("getting flags");
        close(cfd);
        if (verbose) {
            printf("close %d\n", cfd);
        }
        update_conns();
        return;
    }

    r = fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
    if (r < 0) {
        twarn("setting O_NONBLOCK");
        close(cfd);
        if (verbose) {
            printf("close %d\n", cfd);
        }
        update_conns();
        return;
    }

    c = make_conn(cfd, STATE_WANTCOMMAND, default_tube, default_tube);
    if (!c) {
        twarnx("make_conn() failed");
        close(cfd);
        if (verbose) {
            printf("close %d\n", cfd);
        }
        update_conns();
        return;
    }
    c->srv = s;
    c->sock.x = c;
    c->sock.f = (Handle)prothandle;
    c->sock.fd = cfd;

    r = sockwant(&c->sock, 'r');
    if (r == -1) {
        twarn("sockwant");
        close(cfd);
        if (verbose) {
            printf("close %d\n", cfd);
        }
        update_conns();
        return;
    }
    update_conns();
}

void
prot_init()
{
    started_at = nanoseconds();
    memset(op_ct, 0, sizeof(op_ct));

    ms_init(&tubes, NULL, NULL);

    TUBE_ASSIGN(default_tube, tube_find_or_make("default"));
    if (!default_tube) twarnx("Out of memory during startup!");
}

void
prot_replay(Server *s, job list)
{
    job j, nj;
    int64 t, delay;
    int r;

    for (j = list->next ; j != list ; j = nj) {
        nj = j->next;
        job_remove(j);
        walresvupdate(&s->wal, j); /* reserve space for a delete */
        delay = 0;
        switch (j->r.state) {
        case Buried:
            bury_job(s, j, 0);
            break;
        case Delayed:
            t = nanoseconds();
            if (t < j->r.deadline_at) {
                delay = j->r.deadline_at - t;
            }
            /* fall through */
        default:
            r = enqueue_job(s, j, delay, 0);
            if (r < 1) twarnx("error recovering job %"PRIu64, j->r.id);
        }
    }
}
