#include "dat.h"
#include <stdbool.h>
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
#include <sys/utsname.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <stdarg.h>
#include <signal.h>

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
#define CMD_RESERVE_JOB "reserve-job "
#define CMD_DELETE "delete "
#define CMD_RELEASE "release "
#define CMD_BURY "bury "
#define CMD_KICK "kick "
#define CMD_KICKJOB "kick-job "
#define CMD_TOUCH "touch "
#define CMD_STATS "stats"
#define CMD_STATSJOB "stats-job "
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
#define CMD_RESERVE_JOB_LEN CONSTSTRLEN(CMD_RESERVE_JOB)
#define CMD_DELETE_LEN CONSTSTRLEN(CMD_DELETE)
#define CMD_RELEASE_LEN CONSTSTRLEN(CMD_RELEASE)
#define CMD_BURY_LEN CONSTSTRLEN(CMD_BURY)
#define CMD_KICK_LEN CONSTSTRLEN(CMD_KICK)
#define CMD_KICKJOB_LEN CONSTSTRLEN(CMD_KICKJOB)
#define CMD_TOUCH_LEN CONSTSTRLEN(CMD_TOUCH)
#define CMD_STATS_LEN CONSTSTRLEN(CMD_STATS)
#define CMD_STATSJOB_LEN CONSTSTRLEN(CMD_STATSJOB)
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
#define MSG_KICKED "KICKED\r\n"
#define MSG_TOUCHED "TOUCHED\r\n"
#define MSG_BURIED_FMT "BURIED %"PRIu64"\r\n"
#define MSG_INSERTED_FMT "INSERTED %"PRIu64"\r\n"
#define MSG_NOT_IGNORED "NOT_IGNORED\r\n"

#define MSG_OUT_OF_MEMORY "OUT_OF_MEMORY\r\n"
#define MSG_INTERNAL_ERROR "INTERNAL_ERROR\r\n"
#define MSG_DRAINING "DRAINING\r\n"
#define MSG_BAD_FORMAT "BAD_FORMAT\r\n"
#define MSG_UNKNOWN_COMMAND "UNKNOWN_COMMAND\r\n"
#define MSG_EXPECTED_CRLF "EXPECTED_CRLF\r\n"
#define MSG_JOB_TOO_BIG "JOB_TOO_BIG\r\n"

// Connection can be in one of these states:
#define STATE_WANT_COMMAND  0  // conn expects a command from the client
#define STATE_WANT_DATA     1  // conn expects a job data
#define STATE_SEND_JOB      2  // conn sends job to the client
#define STATE_SEND_WORD     3  // conn sends a line reply
#define STATE_WAIT          4  // client awaits for the job reservation
#define STATE_BITBUCKET     5  // conn discards content
#define STATE_CLOSE         6  // conn should be closed
#define STATE_WANT_ENDLINE  7  // skip until the end of a line

#define OP_UNKNOWN 0
#define OP_PUT 1
#define OP_PEEKJOB 2
#define OP_RESERVE 3
#define OP_DELETE 4
#define OP_RELEASE 5
#define OP_BURY 6
#define OP_KICK 7
#define OP_STATS 8
#define OP_STATSJOB 9
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
#define OP_KICKJOB 24
#define OP_RESERVE_JOB 25
#define TOTAL_OPS 26

#define STATS_FMT "---\n" \
    "current-jobs-urgent: %" PRIu64 "\n" \
    "current-jobs-ready: %" PRIu64 "\n" \
    "current-jobs-reserved: %" PRIu64 "\n" \
    "current-jobs-delayed: %u\n" \
    "current-jobs-buried: %" PRIu64 "\n" \
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
    "current-waiting: %" PRIu64 "\n" \
    "total-connections: %u\n" \
    "pid: %ld\n" \
    "version: \"%s\"\n" \
    "rusage-utime: %d.%06d\n" \
    "rusage-stime: %d.%06d\n" \
    "uptime: %u\n" \
    "binlog-oldest-index: %d\n" \
    "binlog-current-index: %d\n" \
    "binlog-records-migrated: %" PRId64 "\n" \
    "binlog-records-written: %" PRId64 "\n" \
    "binlog-max-size: %d\n" \
    "draining: %s\n" \
    "id: %s\n" \
    "hostname: %s\n" \
    "os: %s\n" \
    "platform: %s\n" \
    "\r\n"

#define STATS_TUBE_FMT "---\n" \
    "name: %s\n" \
    "current-jobs-urgent: %" PRIu64 "\n" \
    "current-jobs-ready: %zu\n" \
    "current-jobs-reserved: %" PRIu64 "\n" \
    "current-jobs-delayed: %zu\n" \
    "current-jobs-buried: %" PRIu64 "\n" \
    "total-jobs: %" PRIu64 "\n" \
    "current-using: %u\n" \
    "current-watching: %u\n" \
    "current-waiting: %" PRIu64 "\n" \
    "cmd-delete: %" PRIu64 "\n" \
    "cmd-pause-tube: %" PRIu64 "\n" \
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

// The size of the throw-away (BITBUCKET) buffer. Arbitrary.
#define BUCKET_BUF_SIZE 1024

static uint64 ready_ct = 0;
static uint64 timeout_ct = 0;
static uint64 op_ct[TOTAL_OPS] = {0};
static struct stats global_stat = {0};

static Tube *default_tube;

// If drain_mode is 1, then server does not accept new jobs.
// Variable is set by the SIGUSR1 handler.
static volatile sig_atomic_t drain_mode = 0;

static int64 started_at;

enum { instance_id_bytes = 8 };
static char instance_hex[instance_id_bytes * 2 + 1]; // hex-encoded len of instance_id_bytes

static struct utsname node_info;

// Single linked list with connections that require updates
// in the event notification mechanism.
static Conn *epollq;

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
    CMD_STATSJOB,
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
    CMD_PAUSE_TUBE,
    CMD_KICKJOB,
    CMD_RESERVE_JOB,
};

static Job *remove_buried_job(Job *j);

// epollq_add schedules connection c in the s->conns heap, adds c
// to the epollq list to change expected operation in event notifications.
// rw='w' means to notify when socket is writeable, 'r' - readable, 'h' - closed.
static void
epollq_add(Conn *c, char rw) {
    c->rw = rw;
    connsched(c);
    c->next = epollq;
    epollq = c;
}

// epollq_rmconn removes connection c from the epollq.
static void
epollq_rmconn(Conn *c)
{
    Conn *x, *newhead = NULL;

    while (epollq) {
        // x as next element from epollq.
        x = epollq;
        epollq = epollq->next;
        x->next = NULL;

        // put x back into newhead list.
        if (x != c) {
            x->next = newhead;
            newhead = x;
        }
    }
    epollq = newhead;
}

// Propagate changes to event notification mechanism about expected operations
// in connections' sockets. Clear the epollq list.
static void
epollq_apply()
{
    Conn *c;

    while (epollq) {
        c = epollq;
        epollq = epollq->next;
        c->next = NULL;
        int r = sockwant(&c->sock, c->rw);
        if (r == -1) {
            twarn("sockwant");
            connclose(c);
        }
    }
}

#define reply_msg(c, m) \
    reply((c), (m), CONSTSTRLEN(m), STATE_SEND_WORD)

#define reply_serr(c, e) \
    (twarnx("server error: %s", (e)), reply_msg((c), (e)))

static void
reply(Conn *c, char *line, int len, int state)
{
    if (!c)
        return;

    epollq_add(c, 'w');

    c->reply = line;
    c->reply_len = len;
    c->reply_sent = 0;
    c->state = state;
    if (verbose >= 2) {
        printf(">%d reply %.*s\n", c->sock.fd, len-2, line);
    }
}

static void
reply_line(Conn*, int, const char*, ...)
__attribute__((format(printf, 3, 4)));

// reply_line prints *fmt into c->reply_buffer and
// calls reply() for the string and state.
static void
reply_line(Conn *c, int state, const char *fmt, ...)
{
    int r;
    va_list ap;

    va_start(ap, fmt);
    r = vsnprintf(c->reply_buf, LINE_BUF_SIZE, fmt, ap);
    va_end(ap);

    /* Make sure the buffer was big enough. If not, we have a bug. */
    if (r >= LINE_BUF_SIZE) {
        reply_serr(c, MSG_INTERNAL_ERROR);
        return;
    }

    reply(c, c->reply_buf, r, state);
}

// reply_job tells the connection c which job to send,
// and replies with this line: <msg> <job_id> <job_size>.
static void
reply_job(Conn *c, Job *j, const char *msg)
{
    c->out_job = j;
    c->out_job_sent = 0;
    reply_line(c, STATE_SEND_JOB, "%s %"PRIu64" %u\r\n",
               msg, j->r.id, j->r.body_size - 2);
}

// remove_waiting_conn unsets CONN_TYPE_WAITING for the connection,
// removes it from the waiting_conns set of every tube it's watching.
// Noop if connection is not waiting.
void
remove_waiting_conn(Conn *c)
{
    if (!conn_waiting(c))
        return;

    c->type &= ~CONN_TYPE_WAITING;
    global_stat.waiting_ct--;
    size_t i;
    for (i = 0; i < c->watch.len; i++) {
        Tube *t = c->watch.items[i];
        t->stat.waiting_ct--;
        ms_remove(&t->waiting_conns, c);
    }
}

// enqueue_waiting_conn sets CONN_TYPE_WAITING for the connection,
// adds it to the waiting_conns set of every tube it's watching.
static void
enqueue_waiting_conn(Conn *c)
{
    c->type |= CONN_TYPE_WAITING;
    global_stat.waiting_ct++;
    size_t i;
    for (i = 0; i < c->watch.len; i++) {
        Tube *t = c->watch.items[i];
        t->stat.waiting_ct++;
        ms_append(&t->waiting_conns, c);
    }
}

// next_awaited_job iterates through all the tubes with awaiting connections,
// returns the next ready job with the smallest priority.
// If jobs has the same priority it picks the job with smaller id.
// All tubes with expired pause are unpaused.
static Job *
next_awaited_job(int64 now)
{
    size_t i;
    Job *j = NULL;

    for (i = 0; i < tubes.len; i++) {
        Tube *t = tubes.items[i];
        if (t->pause) {
            if (t->unpause_at > now)
                continue;
            t->pause = 0;
        }
        if (t->waiting_conns.len && t->ready.len) {
            Job *candidate = t->ready.data[0];
            if (!j || job_pri_less(candidate, j)) {
                j = candidate;
            }
        }
    }
    return j;
}

// process_queue performs reservation for every jobs that is awaited for.
static void
process_queue()
{
    Job *j = NULL;
    int64 now = nanoseconds();

    while ((j = next_awaited_job(now))) {
        heapremove(&j->tube->ready, j->heap_index);
        ready_ct--;
        if (j->r.pri < URGENT_THRESHOLD) {
            global_stat.urgent_ct--;
            j->tube->stat.urgent_ct--;
        }

        Conn *c = ms_take(&j->tube->waiting_conns);
        if (c == NULL) {
            twarnx("waiting_conns is empty");
            continue;
        }
        global_stat.reserved_ct++;

        remove_waiting_conn(c);
        conn_reserve_job(c, j);
        reply_job(c, j, MSG_RESERVED);
    }
}

// soonest_delayed_job returns the delayed job
// with the smallest deadline_at among all tubes.
static Job *
soonest_delayed_job()
{
    Job *j = NULL;
    size_t i;

    for (i = 0; i < tubes.len; i++) {
        Tube *t = tubes.items[i];
        if (t->delay.len == 0) {
            continue;
        }
        Job *nj = t->delay.data[0];
        if (!j || nj->r.deadline_at < j->r.deadline_at)
            j = nj;
    }
    return j;
}

// enqueue_job inserts job j in the tube, returns 1 on success, otherwise 0.
// If update_store then it writes an entry to WAL.
// On success it processes the queue.
// BUG: If maintenance of WAL has failed, it is not reported as error.
static int
enqueue_job(Server *s, Job *j, int64 delay, char update_store)
{
    int r;

    j->reserver = NULL;
    if (delay) {
        j->r.deadline_at = nanoseconds() + delay;
        r = heapinsert(&j->tube->delay, j);
        if (!r)
            return 0;
        j->r.state = Delayed;
    } else {
        r = heapinsert(&j->tube->ready, j);
        if (!r)
            return 0;
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

    // The call below makes this function do too much.
    // TODO: refactor this call outside so the call is explicit (not hidden)?
    process_queue();
    return 1;
}

static int
bury_job(Server *s, Job *j, char update_store)
{
    if (update_store) {
        int z = walresvupdate(&s->wal);
        if (!z)
            return 0;
        j->walresv += z;
    }

    job_list_insert(&j->tube->buried, j);
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
enqueue_reserved_jobs(Conn *c)
{
    while (!job_list_is_empty(&c->reserved_jobs)) {
        Job *j = job_list_remove(c->reserved_jobs.next);
        int r = enqueue_job(c->srv, j, 0, 0);
        if (r < 1)
            bury_job(c->srv, j, 0);
        global_stat.reserved_ct--;
        j->tube->stat.reserved_ct--;
        c->soonest_job = NULL;
    }
}

static int
kick_buried_job(Server *s, Job *j)
{
    int r;
    int z;

    z = walresvupdate(&s->wal);
    if (!z)
        return 0;
    j->walresv += z;

    remove_buried_job(j);

    j->r.kick_ct++;
    r = enqueue_job(s, j, 0, 1);
    if (r == 1)
        return 1;

    /* ready queue is full, so bury it */
    bury_job(s, j, 0);
    return 0;
}

static uint
get_delayed_job_ct()
{
    size_t i;
    uint count = 0;

    for (i = 0; i < tubes.len; i++) {
        Tube *t = tubes.items[i];
        count += t->delay.len;
    }
    return count;
}

static int
kick_delayed_job(Server *s, Job *j)
{
    int r;
    int z;

    z = walresvupdate(&s->wal);
    if (!z)
        return 0;
    j->walresv += z;

    heapremove(&j->tube->delay, j->heap_index);

    j->r.kick_ct++;
    r = enqueue_job(s, j, 0, 1);
    if (r == 1)
        return 1;

    /* ready queue is full, so delay it again */
    r = enqueue_job(s, j, j->r.delay, 0);
    if (r == 1)
        return 0;

    /* last resort */
    bury_job(s, j, 0);
    return 0;
}

static int
buried_job_p(Tube *t)
{
    // this function does not do much. inline?
    return !job_list_is_empty(&t->buried);
}

/* return the number of jobs successfully kicked */
static uint
kick_buried_jobs(Server *s, Tube *t, uint n)
{
    uint i;
    for (i = 0; (i < n) && buried_job_p(t); ++i) {
        kick_buried_job(s, t->buried.next);
    }
    return i;
}

/* return the number of jobs successfully kicked */
static uint
kick_delayed_jobs(Server *s, Tube *t, uint n)
{
    uint i;
    for (i = 0; (i < n) && (t->delay.len > 0); ++i) {
        kick_delayed_job(s, (Job *)t->delay.data[0]);
    }
    return i;
}

static uint
kick_jobs(Server *s, Tube *t, uint n)
{
    if (buried_job_p(t))
        return kick_buried_jobs(s, t, n);
    return kick_delayed_jobs(s, t, n);
}

// remove_buried_job returns non-NULL value if job j was in the buried state.
// It excludes the job from the buried list and updates counters.
static Job *
remove_buried_job(Job *j)
{
    if (!j || j->r.state != Buried)
        return NULL;
    j = job_list_remove(j);
    if (j) {
        global_stat.buried_ct--;
        j->tube->stat.buried_ct--;
    }
    return j;
}

// remove_delayed_job returns non-NULL value if job j was in the delayed state.
// It removes the job from the tube delayed heap.
static Job *
remove_delayed_job(Job *j)
{
    if (!j || j->r.state != Delayed)
        return NULL;
    heapremove(&j->tube->delay, j->heap_index);

    return j;
}

// remove_ready_job returns non-NULL value if job j was in the ready state.
// It removes the job from the tube ready heap and updates counters.
static Job *
remove_ready_job(Job *j)
{
    if (!j || j->r.state != Ready)
        return NULL;
    heapremove(&j->tube->ready, j->heap_index);
    ready_ct--;
    if (j->r.pri < URGENT_THRESHOLD) {
        global_stat.urgent_ct--;
        j->tube->stat.urgent_ct--;
    }
    return j;
}

static bool
is_job_reserved_by_conn(Conn *c, Job *j)
{
    return j && j->reserver == c && j->r.state == Reserved;
}

static bool
touch_job(Conn *c, Job *j)
{
    if (is_job_reserved_by_conn(c, j)) {
        j->r.deadline_at = nanoseconds() + j->r.ttr;
        c->soonest_job = NULL;
        return true;
    }
    return false;
}

static void
check_err(Conn *c, const char *s)
{
    if (errno == EAGAIN)
        return;
    if (errno == EINTR)
        return;
    if (errno == EWOULDBLOCK)
        return;

    twarn("%s", s);
    c->state = STATE_CLOSE;
}

/* Scan the given string for the sequence "\r\n" and return the line length.
 * Always returns at least 2 if a match is found. Returns 0 if no match. */
static size_t
scan_line_end(const char *s, int size)
{
    char *match;

    match = memchr(s, '\r', size - 1);
    if (!match)
        return 0;

    /* this is safe because we only scan size - 1 chars above */
    if (match[1] == '\n')
        return match - s + 2;

    return 0;
}

/* parse the command line */
static int
which_cmd(Conn *c)
{
#define TEST_CMD(s,c,o) if (strncmp((s), (c), CONSTSTRLEN(c)) == 0) return (o);
    TEST_CMD(c->cmd, CMD_PUT, OP_PUT);
    TEST_CMD(c->cmd, CMD_PEEKJOB, OP_PEEKJOB);
    TEST_CMD(c->cmd, CMD_PEEK_READY, OP_PEEK_READY);
    TEST_CMD(c->cmd, CMD_PEEK_DELAYED, OP_PEEK_DELAYED);
    TEST_CMD(c->cmd, CMD_PEEK_BURIED, OP_PEEK_BURIED);
    TEST_CMD(c->cmd, CMD_RESERVE_TIMEOUT, OP_RESERVE_TIMEOUT);
    TEST_CMD(c->cmd, CMD_RESERVE_JOB, OP_RESERVE_JOB);
    TEST_CMD(c->cmd, CMD_RESERVE, OP_RESERVE);
    TEST_CMD(c->cmd, CMD_DELETE, OP_DELETE);
    TEST_CMD(c->cmd, CMD_RELEASE, OP_RELEASE);
    TEST_CMD(c->cmd, CMD_BURY, OP_BURY);
    TEST_CMD(c->cmd, CMD_KICK, OP_KICK);
    TEST_CMD(c->cmd, CMD_KICKJOB, OP_KICKJOB);
    TEST_CMD(c->cmd, CMD_TOUCH, OP_TOUCH);
    TEST_CMD(c->cmd, CMD_STATSJOB, OP_STATSJOB);
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
fill_extra_data(Conn *c)
{
    if (!c->sock.fd)
        return; /* the connection was closed */
    if (!c->cmd_len)
        return; /* we don't have a complete command */

    /* how many extra bytes did we read? */
    int64 extra_bytes = c->cmd_read - c->cmd_len;

    int64 job_data_bytes = 0;
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
    int64 cmd_bytes = extra_bytes - job_data_bytes;
    memmove(c->cmd, c->cmd + c->cmd_len + job_data_bytes, cmd_bytes);
    c->cmd_read = cmd_bytes;
    c->cmd_len = 0; /* we no longer know the length of the new command */
}

#define skip(conn,n,msg) (_skip(conn, n, msg, CONSTSTRLEN(msg)))

static void
_skip(Conn *c, int64 n, char *msg, int msglen)
{
    /* Invert the meaning of in_job_read while throwing away data -- it
     * counts the bytes that remain to be thrown away. */
    c->in_job = 0;
    c->in_job_read = n;
    fill_extra_data(c);

    if (c->in_job_read == 0) {
        reply(c, msg, msglen, STATE_SEND_WORD);
        return;
    }

    c->reply = msg;
    c->reply_len = msglen;
    c->reply_sent = 0;
    c->state = STATE_BITBUCKET;
}

static void
enqueue_incoming_job(Conn *c)
{
    int r;
    Job *j = c->in_job;

    c->in_job = NULL; /* the connection no longer owns this job */
    c->in_job_read = 0;

    /* check if the trailer is present and correct */
    if (memcmp(j->body + j->r.body_size - 2, "\r\n", 2)) {
        job_free(j);
        reply_msg(c, MSG_EXPECTED_CRLF);
        return;
    }

    if (verbose >= 2) {
        printf("<%d job %"PRIu64"\n", c->sock.fd, j->r.id);
    }

    if (drain_mode) {
        job_free(j);
        reply_serr(c, MSG_DRAINING);
        return;
    }

    if (j->walresv) {
        reply_serr(c, MSG_INTERNAL_ERROR);
        return;
    }
    j->walresv = walresvput(&c->srv->wal, j);
    if (!j->walresv) {
        reply_serr(c, MSG_OUT_OF_MEMORY);
        return;
    }

    /* we have a complete job, so let's stick it in the pqueue */
    r = enqueue_job(c->srv, j, j->r.delay, 1);

    // Dead code: condition cannot happen, r can take 1 or 0 values only.
    if (r < 0) {
        reply_serr(c, MSG_INTERNAL_ERROR);
        return;
    }

    global_stat.total_jobs_ct++;
    j->tube->stat.total_jobs_ct++;

    if (r == 1) {
        reply_line(c, STATE_SEND_WORD, MSG_INSERTED_FMT, j->r.id);
        return;
    }

    /* out of memory trying to grow the queue, so it gets buried */
    bury_job(c->srv, j, 0);
    reply_line(c, STATE_SEND_WORD, MSG_BURIED_FMT, j->r.id);
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
    Server *s = x;
    struct rusage ru;

    s = x;

    if (s->wal.head) {
        whead = s->wal.head->seq;
    }

    if (s->wal.cur) {
        wcur = s->wal.cur->seq;
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
                    op_ct[OP_STATSJOB],
                    op_ct[OP_STATS_TUBE],
                    op_ct[OP_LIST_TUBES],
                    op_ct[OP_LIST_TUBE_USED],
                    op_ct[OP_LIST_TUBES_WATCHED],
                    op_ct[OP_PAUSE_TUBE],
                    timeout_ct,
                    global_stat.total_jobs_ct,
                    job_data_size_limit,
                    tubes.len,
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
                    s->wal.nmig,
                    s->wal.nrec,
                    s->wal.filesize,
                    drain_mode ? "true" : "false",
                    instance_hex,
                    node_info.nodename,
                    node_info.version,
                    node_info.machine);
}

/* Read an integer from the given buffer and place it in num.
 * Parsed integer should fit into uint64.
 * Update end to point to the address after the last character consumed.
 * num and end can be NULL. If they are both NULL, read_u64() will do the
 * conversion and return the status code but not update any values.
 * This is an easy way to check for errors.
 * If end is NULL, read_u64() will also check that the entire input string
 * was consumed and return an error code otherwise.
 * Return 0 on success, or nonzero on failure.
 * If a failure occurs, num and end are not modified. */
static int
read_u64(uint64 *num, const char *buf, char **end)
{
    uintmax_t tnum;
    char *tend;

    errno = 0;
    while (buf[0] == ' ')
        buf++;
    if (buf[0] < '0' || '9' < buf[0])
        return -1;
    tnum = strtoumax(buf, &tend, 10);
    if (tend == buf)
        return -1;
    if (errno)
        return -1;
    if (!end && tend[0] != '\0')
        return -1;
    if (tnum > UINT64_MAX)
        return -1;

    if (num) *num = (uint64)tnum;
    if (end) *end = tend;
    return 0;
}

// Indentical to read_u64() but instead reads into uint32.
static int
read_u32(uint32 *num, const char *buf, char **end)
{
    uintmax_t tnum;
    char *tend;

    errno = 0;
    while (buf[0] == ' ')
        buf++;
    if (buf[0] < '0' || '9' < buf[0])
        return -1;
    tnum = strtoumax(buf, &tend, 10);
    if (tend == buf)
        return -1;
    if (errno)
        return -1;
    if (!end && tend[0] != '\0')
        return -1;
    if (tnum > UINT32_MAX)
        return -1;

    if (num) *num = (uint32)tnum;
    if (end) *end = tend;
    return 0;
}

/* Read a delay value in seconds from the given buffer and
   place it in duration in nanoseconds.
   The interface and behavior are analogous to read_u32(). */
static int
read_duration(int64 *duration, const char *buf, char **end)
{
    int r;
    uint32 dur_sec;

    r = read_u32(&dur_sec, buf, end);
    if (r)
        return r;
    *duration = ((int64) dur_sec) * 1000000000;
    return 0;
}

/* Read a tube name from the given buffer moving the buffer to the name start */
static int
read_tube_name(char **tubename, char *buf, char **end)
{
    size_t len;

    while (buf[0] == ' ')
        buf++;
    len = strspn(buf, NAME_CHARS);
    if (len == 0)
        return -1;
    if (tubename)
        *tubename = buf;
    if (end)
        *end = buf + len;
    return 0;
}

static void
wait_for_job(Conn *c, int timeout)
{
    c->state = STATE_WAIT;
    enqueue_waiting_conn(c);

    /* Set the pending timeout to the requested timeout amount */
    c->pending_timeout = timeout;

    // only care if they hang up
    epollq_add(c, 'h');
}

typedef int(*fmt_fn)(char *, size_t, void *);

static void
do_stats(Conn *c, fmt_fn fmt, void *data)
{
    int r, stats_len;

    /* first, measure how big a buffer we will need */
    stats_len = fmt(NULL, 0, data) + 16;

    c->out_job = allocate_job(stats_len); /* fake job to hold stats data */
    if (!c->out_job) {
        reply_serr(c, MSG_OUT_OF_MEMORY);
        return;
    }

    /* Mark this job as a copy so it can be appropriately freed later on */
    c->out_job->r.state = Copy;

    /* now actually format the stats data */
    r = fmt(c->out_job->body, stats_len, data);
    /* and set the actual body size */
    c->out_job->r.body_size = r;
    if (r > stats_len) {
        reply_serr(c, MSG_INTERNAL_ERROR);
        return;
    }

    c->out_job_sent = 0;
    reply_line(c, STATE_SEND_JOB, "OK %d\r\n", r - 2);
}

static void
do_list_tubes(Conn *c, Ms *l)
{
    char *buf;
    Tube *t;
    size_t i, resp_z;

    /* first, measure how big a buffer we will need */
    resp_z = 6; /* initial "---\n" and final "\r\n" */
    for (i = 0; i < l->len; i++) {
        t = l->items[i];
        resp_z += 3 + strlen(t->name); /* including "- " and "\n" */
    }

    c->out_job = allocate_job(resp_z); /* fake job to hold response data */
    if (!c->out_job) {
        reply_serr(c, MSG_OUT_OF_MEMORY);
        return;
    }

    /* Mark this job as a copy so it can be appropriately freed later on */
    c->out_job->r.state = Copy;

    /* now actually format the response */
    buf = c->out_job->body;
    buf += snprintf(buf, 5, "---\n");
    for (i = 0; i < l->len; i++) {
        t = l->items[i];
        buf += snprintf(buf, 4 + strlen(t->name), "- %s\n", t->name);
    }
    buf[0] = '\r';
    buf[1] = '\n';

    c->out_job_sent = 0;
    reply_line(c, STATE_SEND_JOB, "OK %zu\r\n", resp_z - 2);
}

static int
fmt_job_stats(char *buf, size_t size, Job *j)
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
fmt_stats_tube(char *buf, size_t size, Tube *t)
{
    uint64 time_left;

    if (t->pause > 0) {
        time_left = (t->unpause_at - nanoseconds()) / 1000000000;
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
maybe_enqueue_incoming_job(Conn *c)
{
    Job *j = c->in_job;

    /* do we have a complete job? */
    if (c->in_job_read == j->r.body_size) {
        enqueue_incoming_job(c);
        return;
    }

    /* otherwise we have incomplete data, so just keep waiting */
    c->state = STATE_WANT_DATA;
}

/* j can be NULL */
static Job *
remove_this_reserved_job(Conn *c, Job *j)
{
    j = job_list_remove(j);
    if (j) {
        global_stat.reserved_ct--;
        j->tube->stat.reserved_ct--;
        j->reserver = NULL;
    }
    c->soonest_job = NULL;
    return j;
}

static Job *
remove_reserved_job(Conn *c, Job *j)
{
    if (!is_job_reserved_by_conn(c, j))
        return NULL;
    return remove_this_reserved_job(c, j);
}

static bool
is_valid_tube(const char *name, size_t max)
{
    size_t len = strlen(name);
    return 0 < len && len <= max &&
        strspn(name, NAME_CHARS) == len &&
        name[0] != '-';
}

static void
dispatch_cmd(Conn *c)
{
    int r, timeout = -1;
    uint i;
    uint count;
    Job *j = 0;
    byte type;
    char *size_buf, *delay_buf, *ttr_buf, *pri_buf, *end_buf, *name;
    uint32 pri;
    uint32 body_size;
    int64 delay, ttr;
    uint64 id;
    Tube *t = NULL;

    /* NUL-terminate this string so we can use strtol and friends */
    c->cmd[c->cmd_len - 2] = '\0';

    /* check for possible maliciousness */
    if (strlen(c->cmd) != c->cmd_len - 2) {
        reply_msg(c, MSG_BAD_FORMAT);
        return;
    }

    type = which_cmd(c);
    if (verbose >= 2) {
        printf("<%d command %s\n", c->sock.fd, op_names[type]);
    }

    switch (type) {
    case OP_PUT:
        if (read_u32(&pri, c->cmd + 4, &delay_buf) ||
            read_duration(&delay, delay_buf, &ttr_buf) ||
            read_duration(&ttr, ttr_buf, &size_buf) ||
            read_u32(&body_size, size_buf, &end_buf)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        if (body_size > job_data_size_limit) {
            /* throw away the job body and respond with JOB_TOO_BIG */
            skip(c, (int64)body_size + 2, MSG_JOB_TOO_BIG);
            return;
        }

        /* don't allow trailing garbage */
        if (end_buf[0] != '\0') {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }

        connsetproducer(c);

        if (ttr < 1000000000) {
            ttr = 1000000000;
        }

        c->in_job = make_job(pri, delay, ttr, body_size + 2, c->use);

        /* OOM? */
        if (!c->in_job) {
            /* throw away the job body and respond with OUT_OF_MEMORY */
            twarnx("server error: " MSG_OUT_OF_MEMORY);
            skip(c, body_size + 2, MSG_OUT_OF_MEMORY);
            return;
        }

        fill_extra_data(c);

        /* it's possible we already have a complete job */
        maybe_enqueue_incoming_job(c);
        return;

    case OP_PEEK_READY:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_PEEK_READY_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        if (c->use->ready.len) {
            j = job_copy(c->use->ready.data[0]);
        }

        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }
        reply_job(c, j, MSG_FOUND);
        return;

    case OP_PEEK_DELAYED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_PEEK_DELAYED_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        if (c->use->delay.len) {
            j = job_copy(c->use->delay.data[0]);
        }

        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }
        reply_job(c, j, MSG_FOUND);
        return;

    case OP_PEEK_BURIED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_PEEK_BURIED_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        if (buried_job_p(c->use))
            j = job_copy(c->use->buried.next);
        else
            j = NULL;

        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }
        reply_job(c, j, MSG_FOUND);
        return;

    case OP_PEEKJOB:
        if (read_u64(&id, c->cmd + CMD_PEEKJOB_LEN, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        /* So, peek is annoying, because some other connection might free the
         * job while we are still trying to write it out. So we copy it and
         * free the copy when it's done sending, in the "conn_want_command" function. */
        j = job_copy(job_find(id));

        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }
        reply_job(c, j, MSG_FOUND);
        return;

    case OP_RESERVE_TIMEOUT:
        errno = 0;
        timeout = strtol(c->cmd + CMD_RESERVE_TIMEOUT_LEN, &end_buf, 10);
        if (errno) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        /* Falls through */

    case OP_RESERVE:
        /* don't allow trailing garbage */
        if (type == OP_RESERVE && c->cmd_len != CMD_RESERVE_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;
        connsetworker(c);

        if (conndeadlinesoon(c) && !conn_ready(c)) {
            reply_msg(c, MSG_DEADLINE_SOON);
            return;
        }

        /* try to get a new job for this guy */
        wait_for_job(c, timeout);
        process_queue();
        return;

    case OP_RESERVE_JOB:
        if (read_u64(&id, c->cmd + CMD_RESERVE_JOB_LEN, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        // This command could produce "deadline soon" if
        // the connection has a reservation about to expire.
        // We choose not to do it, because this command does not block
        // for an arbitrary amount of time as reserve and reserve-with-timeout.

        j = job_find(id);
        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }
        // Check if this job is already reserved.
        if (j->r.state == Reserved || j->r.state == Invalid) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }

        // Job can be in ready, buried or delayed states.
        if (j->r.state == Ready) {
            j = remove_ready_job(j);
        } else if (j->r.state == Buried) {
            j = remove_buried_job(j);
        } else if (j->r.state == Delayed) {
            j = remove_delayed_job(j);
        } else {
            reply_serr(c, MSG_INTERNAL_ERROR);
            return;
        }

        connsetworker(c);
        global_stat.reserved_ct++;

        conn_reserve_job(c, j);
        reply_job(c, j, MSG_RESERVED);
        return;

    case OP_DELETE:
        if (read_u64(&id, c->cmd + CMD_DELETE_LEN, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        {
            Job *jf = job_find(id);
            j = remove_reserved_job(c, jf);
            if (!j)
                j = remove_ready_job(jf);
            if (!j)
                j = remove_buried_job(jf);
            if (!j)
                j = remove_delayed_job(jf);
        }

        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }

        j->tube->stat.total_delete_ct++;

        j->r.state = Invalid;
        r = walwrite(&c->srv->wal, j);
        walmaint(&c->srv->wal);
        job_free(j);

        if (!r) {
            reply_serr(c, MSG_INTERNAL_ERROR);
            return;
        }
        reply_msg(c, MSG_DELETED);
        return;

    case OP_RELEASE:
        if (read_u64(&id, c->cmd + CMD_RELEASE_LEN, &pri_buf) ||
            read_u32(&pri, pri_buf, &delay_buf) ||
            read_duration(&delay, delay_buf, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        j = remove_reserved_job(c, job_find(id));

        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }

        /* We want to update the delay deadline on disk, so reserve space for
         * that. */
        if (delay) {
            int z = walresvupdate(&c->srv->wal);
            if (!z) {
                reply_serr(c, MSG_OUT_OF_MEMORY);
                return;
            }
            j->walresv += z;
        }

        j->r.pri = pri;
        j->r.delay = delay;
        j->r.release_ct++;

        r = enqueue_job(c->srv, j, delay, !!delay);
        if (r < 0) {
            reply_serr(c, MSG_INTERNAL_ERROR);
            return;
        }
        if (r == 1) {
            reply_msg(c, MSG_RELEASED);
            return;
        }

        /* out of memory trying to grow the queue, so it gets buried */
        bury_job(c->srv, j, 0);
        reply_msg(c, MSG_BURIED);
        return;

    case OP_BURY:
        if (read_u64(&id, c->cmd + CMD_BURY_LEN, &pri_buf) ||
            read_u32(&pri, pri_buf, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }

        op_ct[type]++;

        j = remove_reserved_job(c, job_find(id));

        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }

        j->r.pri = pri;
        r = bury_job(c->srv, j, 1);
        if (!r) {
            reply_serr(c, MSG_INTERNAL_ERROR);
            return;
        }
        reply_msg(c, MSG_BURIED);
        return;

    case OP_KICK:
        errno = 0;
        count = strtoul(c->cmd + CMD_KICK_LEN, &end_buf, 10);
        if (end_buf == c->cmd + CMD_KICK_LEN || errno) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }

        op_ct[type]++;

        i = kick_jobs(c->srv, c->use, count);
        reply_line(c, STATE_SEND_WORD, "KICKED %u\r\n", i);
        return;

    case OP_KICKJOB:
        if (read_u64(&id, c->cmd + CMD_KICKJOB_LEN, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }

        op_ct[type]++;

        j = job_find(id);
        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }

        if ((j->r.state == Buried && kick_buried_job(c->srv, j)) ||
            (j->r.state == Delayed && kick_delayed_job(c->srv, j))) {
            reply_msg(c, MSG_KICKED);
        } else {
            reply_msg(c, MSG_NOTFOUND);
        }
        return;

    case OP_TOUCH:
        if (read_u64(&id, c->cmd + CMD_TOUCH_LEN, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        if (touch_job(c, job_find(id))) {
            reply_msg(c, MSG_TOUCHED);
        } else {
            reply_msg(c, MSG_NOTFOUND);
        }
        return;

    case OP_STATS:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_STATS_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        do_stats(c, fmt_stats, c->srv);
        return;

    case OP_STATSJOB:
        if (read_u64(&id, c->cmd + CMD_STATSJOB_LEN, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        j = job_find(id);
        if (!j) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }

        if (!j->tube) {
            reply_serr(c, MSG_INTERNAL_ERROR);
            return;
        }
        do_stats(c, (fmt_fn) fmt_job_stats, j);
        return;

    case OP_STATS_TUBE:
        name = c->cmd + CMD_STATS_TUBE_LEN;
        if (!is_valid_tube(name, MAX_TUBE_NAME_LEN - 1)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        t = tube_find(name);
        if (!t) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }
        do_stats(c, (fmt_fn) fmt_stats_tube, t);
        t = NULL;
        return;

    case OP_LIST_TUBES:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_LIST_TUBES_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;
        do_list_tubes(c, &tubes);
        return;

    case OP_LIST_TUBE_USED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_LIST_TUBE_USED_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;
        reply_line(c, STATE_SEND_WORD, "USING %s\r\n", c->use->name);
        return;

    case OP_LIST_TUBES_WATCHED:
        /* don't allow trailing garbage */
        if (c->cmd_len != CMD_LIST_TUBES_WATCHED_LEN + 2) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;
        do_list_tubes(c, &c->watch);
        return;

    case OP_USE:
        name = c->cmd + CMD_USE_LEN;
        if (!is_valid_tube(name, MAX_TUBE_NAME_LEN - 1)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        TUBE_ASSIGN(t, tube_find_or_make(name));
        if (!t) {
            reply_serr(c, MSG_OUT_OF_MEMORY);
            return;
        }

        c->use->using_ct--;
        TUBE_ASSIGN(c->use, t);
        TUBE_ASSIGN(t, NULL);
        c->use->using_ct++;

        reply_line(c, STATE_SEND_WORD, "USING %s\r\n", c->use->name);
        return;

    case OP_WATCH:
        name = c->cmd + CMD_WATCH_LEN;
        if (!is_valid_tube(name, MAX_TUBE_NAME_LEN - 1)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        TUBE_ASSIGN(t, tube_find_or_make(name));
        if (!t) {
            reply_serr(c, MSG_OUT_OF_MEMORY);
            return;
        }

        r = 1;
        if (!ms_contains(&c->watch, t))
            r = ms_append(&c->watch, t);
        TUBE_ASSIGN(t, NULL);
        if (!r) {
            reply_serr(c, MSG_OUT_OF_MEMORY);
            return;
        }
        reply_line(c, STATE_SEND_WORD, "WATCHING %zu\r\n", c->watch.len);
        return;

    case OP_IGNORE:
        name = c->cmd + CMD_IGNORE_LEN;
        if (!is_valid_tube(name, MAX_TUBE_NAME_LEN - 1)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        t = NULL;
        for (i = 0; i < c->watch.len; i++) {
            t = c->watch.items[i];
            if (strncmp(t->name, name, MAX_TUBE_NAME_LEN) == 0)
                break;
            t = NULL;
        }

        if (t && c->watch.len < 2) {
            reply_msg(c, MSG_NOT_IGNORED);
            return;
        }

        if (t)
            ms_remove(&c->watch, t); /* may free t if refcount => 0 */
        t = NULL;
        reply_line(c, STATE_SEND_WORD, "WATCHING %zu\r\n", c->watch.len);
        return;

    case OP_QUIT:
        c->state = STATE_CLOSE;
        return;

    case OP_PAUSE_TUBE:
        if (read_tube_name(&name, c->cmd + CMD_PAUSE_TUBE_LEN, &delay_buf) ||
            read_duration(&delay, delay_buf, NULL)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        op_ct[type]++;

        *delay_buf = '\0';
        if (!is_valid_tube(name, MAX_TUBE_NAME_LEN - 1)) {
            reply_msg(c, MSG_BAD_FORMAT);
            return;
        }
        t = tube_find(name);
        if (!t) {
            reply_msg(c, MSG_NOTFOUND);
            return;
        }

        // Always pause for a positive amount of time, to make sure
        // that waiting clients wake up when the deadline arrives.
        if (delay == 0) {
            delay = 1;
        }

        t->unpause_at = nanoseconds() + delay;
        t->pause = delay;
        t->stat.pause_ct++;

        reply_line(c, STATE_SEND_WORD, "PAUSED\r\n");
        return;

    default:
        reply_msg(c, MSG_UNKNOWN_COMMAND);
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
conn_timeout(Conn *c)
{
    int should_timeout = 0;
    Job *j;

    /* Check if the client was trying to reserve a job. */
    if (conn_waiting(c) && conndeadlinesoon(c))
        should_timeout = 1;

    /* Check if any reserved jobs have run out of time. We should do this
     * whether or not the client is waiting for a new reservation. */
    while ((j = connsoonestjob(c))) {
        if (j->r.deadline_at >= nanoseconds())
            break;

        /* This job is in the middle of being written out. If we return it to
         * the ready queue, someone might free it before we finish writing it
         * out to the socket. So we'll copy it here and free the copy when it's
         * done sending. */
        if (j == c->out_job) {
            c->out_job = job_copy(c->out_job);
        }

        timeout_ct++; /* stats */
        j->r.timeout_ct++;
        int r = enqueue_job(c->srv, remove_this_reserved_job(c, j), 0, 0);
        if (r < 1)
            bury_job(c->srv, j, 0); /* out of memory, so bury it */
        connsched(c);
    }

    if (should_timeout) {
        remove_waiting_conn(c);
        reply_msg(c, MSG_DEADLINE_SOON);
    } else if (conn_waiting(c) && c->pending_timeout >= 0) {
        c->pending_timeout = -1;
        remove_waiting_conn(c);
        reply_msg(c, MSG_TIMED_OUT);
    }
}

void
enter_drain_mode(int sig)
{
    UNUSED_PARAMETER(sig);
    drain_mode = 1;
}

static void
conn_want_command(Conn *c)
{
    epollq_add(c, 'r');

    /* was this a peek or stats command? */
    if (c->out_job && c->out_job->r.state == Copy)
        job_free(c->out_job);
    c->out_job = NULL;

    c->reply_sent = 0; /* now that we're done, reset this */
    c->state = STATE_WANT_COMMAND;
}

static void
conn_process_io(Conn *c)
{
    int r;
    int64 to_read;
    Job *j;
    struct iovec iov[2];

    switch (c->state) {
    case STATE_WANT_COMMAND:
        r = read(c->sock.fd, c->cmd + c->cmd_read, LINE_BUF_SIZE - c->cmd_read);
        if (r == -1) {
            check_err(c, "read()");
            return;
        }
        if (r == 0) {
            c->state = STATE_CLOSE;
            return;
        }

        c->cmd_read += r;
        c->cmd_len = scan_line_end(c->cmd, c->cmd_read);
        if (c->cmd_len) {
            // We found complete command line. Bail out to h_conn.
            return;
        }

        // c->cmd_read > LINE_BUF_SIZE can't happen

        if (c->cmd_read == LINE_BUF_SIZE) {
            // Command line too long.
            // Put connection into special state that discards
            // the command line until the end line is found.
            c->cmd_read = 0; // discard the input so far
            c->state = STATE_WANT_ENDLINE;
        }
        // We have an incomplete line, so just keep waiting.
        return;

    case STATE_WANT_ENDLINE:
        r = read(c->sock.fd, c->cmd + c->cmd_read, LINE_BUF_SIZE - c->cmd_read);
        if (r == -1) {
            check_err(c, "read()");
            return;
        }
        if (r == 0) {
            c->state = STATE_CLOSE;
            return;
        }

        c->cmd_read += r;
        c->cmd_len = scan_line_end(c->cmd, c->cmd_read);
        if (c->cmd_len) {
            // Found the EOL. Reply and reuse whatever was read afer the EOL.
            reply_msg(c, MSG_BAD_FORMAT);
            fill_extra_data(c);
            return;
        }

        // c->cmd_read > LINE_BUF_SIZE can't happen

        if (c->cmd_read == LINE_BUF_SIZE) {
            // Keep discarding the input since no EOL was found.
            c->cmd_read = 0;
        }
        return;

    case STATE_BITBUCKET: {
        /* Invert the meaning of in_job_read while throwing away data -- it
         * counts the bytes that remain to be thrown away. */
        static char bucket[BUCKET_BUF_SIZE];
        to_read = min(c->in_job_read, BUCKET_BUF_SIZE);
        r = read(c->sock.fd, bucket, to_read);
        if (r == -1) {
            check_err(c, "read()");
            return;
        }
        if (r == 0) {
            c->state = STATE_CLOSE;
            return;
        }

        c->in_job_read -= r; /* we got some bytes */

        /* (c->in_job_read < 0) can't happen */

        if (c->in_job_read == 0) {
            reply(c, c->reply, c->reply_len, STATE_SEND_WORD);
        }
        return;
    }
    case STATE_WANT_DATA:
        j = c->in_job;

        r = read(c->sock.fd, j->body + c->in_job_read, j->r.body_size -c->in_job_read);
        if (r == -1) {
            check_err(c, "read()");
            return;
        }
        if (r == 0) {
            c->state = STATE_CLOSE;
            return;
        }

        c->in_job_read += r; /* we got some bytes */

        /* (j->in_job_read > j->r.body_size) can't happen */

        maybe_enqueue_incoming_job(c);
        return;
    case STATE_SEND_WORD:
        r= write(c->sock.fd, c->reply + c->reply_sent, c->reply_len - c->reply_sent);
        if (r == -1) {
            check_err(c, "write()");
            return;
        }
        if (r == 0) {
            c->state = STATE_CLOSE;
            return;
        }

        c->reply_sent += r; /* we got some bytes */

        /* (c->reply_sent > c->reply_len) can't happen */

        if (c->reply_sent == c->reply_len) {
            conn_want_command(c);
            return;
        }

        /* otherwise we sent an incomplete reply, so just keep waiting */
        break;
    case STATE_SEND_JOB:
        j = c->out_job;

        iov[0].iov_base = (void *)(c->reply + c->reply_sent);
        iov[0].iov_len = c->reply_len - c->reply_sent; /* maybe 0 */
        iov[1].iov_base = j->body + c->out_job_sent;
        iov[1].iov_len = j->r.body_size - c->out_job_sent;

        r = writev(c->sock.fd, iov, 2);
        if (r == -1) {
            check_err(c, "writev()");
            return;
        }
        if (r == 0) {
            c->state = STATE_CLOSE;
            return;
        }

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
            conn_want_command(c);
            return;
        }

        /* otherwise we sent incomplete data, so just keep waiting */
        break;
    case STATE_WAIT:
        if (c->halfclosed) {
            c->pending_timeout = -1;
            remove_waiting_conn(c);
            reply_msg(c, MSG_TIMED_OUT);
            return;
        }
        break;
    }
}

#define want_command(c) ((c)->sock.fd && ((c)->state == STATE_WANT_COMMAND))
#define cmd_data_ready(c) (want_command(c) && (c)->cmd_read)

static void
h_conn(const int fd, const short which, Conn *c)
{
    if (fd != c->sock.fd) {
        twarnx("Argh! event fd doesn't match conn fd.");
        close(fd);
        connclose(c);
        epollq_apply();
        return;
    }

    if (which == 'h') {
        c->halfclosed = 1;
    }

    conn_process_io(c);
    while (cmd_data_ready(c) && (c->cmd_len = scan_line_end(c->cmd, c->cmd_read))) {
        dispatch_cmd(c);
        fill_extra_data(c);
    }
    if (c->state == STATE_CLOSE) {
        epollq_rmconn(c);
        connclose(c);
    }
    epollq_apply();
}

static void
prothandle(Conn *c, int ev)
{
    h_conn(c->sock.fd, ev, c);
}

// prottick returns nanoseconds till the next work.
int64
prottick(Server *s)
{
    Job *j;
    int64 now;
    Tube *t;
    int64 period = 0x34630B8A000LL; /* 1 hour in nanoseconds */
    int64 d;

    now = nanoseconds();

    // Enqueue all jobs that are no longer delayed.
    // Capture the smallest period from the soonest delayed job.
    while ((j = soonest_delayed_job())) {
        d = j->r.deadline_at - now;
        if (d > 0) {
            period = min(period, d);
            break;
        }
        heapremove(&j->tube->delay, j->heap_index);
        int r = enqueue_job(s, j, 0, 0);
        if (r < 1)
            bury_job(s, j, 0);  /* out of memory */
    }

    // Unpause every possible tube and process the queue.
    // Capture the smallest period from the soonest pause deadline.
    size_t i;
    for (i = 0; i < tubes.len; i++) {
        t = tubes.items[i];
        d = t->unpause_at - now;
        if (t->pause && d <= 0) {
            t->pause = 0;
            process_queue();
        }
        else if (d > 0) {
            period = min(period, d);
        }
    }

    // Process connections with pending timeouts. Release jobs with expired ttr.
    // Capture the smallest period from the soonest connection.
    while (s->conns.len) {
        Conn *c = s->conns.data[0];
        d = c->tickat - now;
        if (d > 0) {
            period = min(period, d);
            break;
        }
        heapremove(&s->conns, 0);
        c->in_conns = 0;
        conn_timeout(c);
    }

    epollq_apply();

    return period;
}

void
h_accept(const int fd, const short which, Server *s)
{
    UNUSED_PARAMETER(which);
    struct sockaddr_storage addr;

    socklen_t addrlen = sizeof addr;
    int cfd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if (cfd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) twarn("accept()");
        epollq_apply();
        return;
    }
    if (verbose) {
        printf("accept %d\n", cfd);
    }

    int flags = fcntl(cfd, F_GETFL, 0);
    if (flags < 0) {
        twarn("getting flags");
        close(cfd);
        if (verbose) {
            printf("close %d\n", cfd);
        }
        epollq_apply();
        return;
    }

    int r = fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
    if (r < 0) {
        twarn("setting O_NONBLOCK");
        close(cfd);
        if (verbose) {
            printf("close %d\n", cfd);
        }
        epollq_apply();
        return;
    }

    Conn *c = make_conn(cfd, STATE_WANT_COMMAND, default_tube, default_tube);
    if (!c) {
        twarnx("make_conn() failed");
        close(cfd);
        if (verbose) {
            printf("close %d\n", cfd);
        }
        epollq_apply();
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
    }
    epollq_apply();
}

void
prot_init()
{
    started_at = nanoseconds();
    memset(op_ct, 0, sizeof(op_ct));

    int dev_random = open("/dev/urandom", O_RDONLY);
    if (dev_random < 0) {
        twarn("open /dev/urandom");
        exit(50);
    }

    int i, r;
    byte rand_data[instance_id_bytes];
    r = read(dev_random, &rand_data, instance_id_bytes);
    if (r != instance_id_bytes) {
        twarn("read /dev/urandom");
        exit(50);
    }
    for (i = 0; i < instance_id_bytes; i++) {
        sprintf(instance_hex + (i * 2), "%02x", rand_data[i]);
    }
    close(dev_random);

    if (uname(&node_info) == -1) {
        warn("uname");
        exit(50);
    }

    ms_init(&tubes, NULL, NULL);

    TUBE_ASSIGN(default_tube, tube_find_or_make("default"));
    if (!default_tube)
        twarnx("Out of memory during startup!");
}

// For each job in list, inserts the job into the appropriate data
// structures and adds it to the log.
//
// Returns 1 on success, 0 on failure.
int
prot_replay(Server *s, Job *list)
{
    Job *j, *nj;
    int64 t;
    int r;

    for (j = list->next ; j != list ; j = nj) {
        nj = j->next;
        job_list_remove(j);
        int z = walresvupdate(&s->wal);
        if (!z) {
            twarnx("failed to reserve space");
            return 0;
        }
        int64 delay = 0;
        switch (j->r.state) {
        case Buried: {
            bury_job(s, j, 0);
            break;
        }
        case Delayed:
            t = nanoseconds();
            if (t < j->r.deadline_at) {
                delay = j->r.deadline_at - t;
            }
            /* Falls through */
        default:
            r = enqueue_job(s, j, delay, 0);
            if (r < 1)
                twarnx("error recovering job %"PRIu64, j->r.id);
        }
    }
    return 1;
}
