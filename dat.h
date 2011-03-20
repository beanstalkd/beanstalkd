#define int32_t  do_not_use_int32_t
#define uint32_t do_not_use_uint32_t
#define int64_t  do_not_use_int64_t
#define uint64_t do_not_use_uint64_t

typedef uint64      usec;
typedef struct ms   *ms;
typedef struct pq   *pq;
typedef struct job  *job;
typedef struct tube *tube;
typedef struct conn *conn;

typedef void(*evh)(int, short, void *);
typedef int(*job_cmp_fn)(job, job);
typedef void(*ms_event_fn)(ms a, void *item, size_t i);

#if _LP64
#define NUM_PRIMES 48
#else
#define NUM_PRIMES 19
#endif

#define MAX_TUBE_NAME_LEN 201

/* A command can be at most LINE_BUF_SIZE chars, including "\r\n". This value
 * MUST be enough to hold the longest possible command or reply line, which is
 * currently "USING a{200}\r\n". */
#define LINE_BUF_SIZE 208

#define JOB_STATE_INVALID  0
#define JOB_STATE_READY    1
#define JOB_STATE_RESERVED 2
#define JOB_STATE_BURIED   3
#define JOB_STATE_DELAYED  4
#define JOB_STATE_COPY     5

/* CONN_TYPE_* are bit masks */
#define CONN_TYPE_PRODUCER 1
#define CONN_TYPE_WORKER   2
#define CONN_TYPE_WAITING  4

#define USEC   (1)
#define MSEC   (1000 * USEC)
#define SECOND (1000 * MSEC)

#define min(a,b) ((a)<(b)?(a):(b))

#define twarn(fmt, args...) warn("%s:%d in %s: " fmt, \
                                 __FILE__, __LINE__, __func__, ##args)
#define twarnx(fmt, args...) warnx("%s:%d in %s: " fmt, \
                                   __FILE__, __LINE__, __func__, ##args)

#ifdef DEBUG
#define dbgprintf(fmt, args...) ((void) fprintf(stderr, fmt, ##args))
#else
#define dbgprintf(fmt, ...) ((void) 0)
#endif

#define URGENT_THRESHOLD 1024
#define JOB_DATA_SIZE_LIMIT_DEFAULT ((1 << 16) - 1)

struct stats {
    uint urgent_ct;
    uint waiting_ct;
    uint buried_ct;
    uint reserved_ct;
    uint pause_ct;
    uint64   total_jobs_ct;
};

struct pq {
    uint cap;
    uint used;
    job_cmp_fn cmp;
    job *heap;
};

struct ms {
    size_t used, cap, last;
    void **items;
    ms_event_fn oninsert, onremove;
};

/* If you modify this struct, you MUST increment binlog format version in
 * binlog.c. */
struct job {

    /* persistent fields; these get written to the binlog */
    uint64 id;
    uint32 pri;
    usec delay;
    usec ttr;
    int32 body_size;
    usec created_at;
    usec deadline_at;
    uint32 reserve_ct;
    uint32 timeout_ct;
    uint32 release_ct;
    uint32 bury_ct;
    uint32 kick_ct;
    uint8_t state;

    /* bookeeping fields; these are in-memory only */
    char pad[6];
    tube tube;
    job prev, next; /* linked list of jobs */
    job ht_next; /* Next job in a hash table list */
    size_t heap_index; /* where is this job in its current heap */
    void *binlog;
    void *reserver;
    size_t reserved_binlog_space;

    /* variable-size job data; written separately to the binlog */
    char body[];
};

struct tube {
    uint refs;
    char name[MAX_TUBE_NAME_LEN];
    struct pq ready;
    struct pq delay;
    struct job buried;
    struct ms waiting; /* set of conns */
    struct stats stat;
    uint using_ct;
    uint watching_ct;
    usec pause;
    usec deadline_at;
};

struct conn {
    conn prev, next; /* linked list of connections */
    int fd;
    char state;
    char type;
    struct event evq;
    int evmask;
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

void v();

void warn(const char *fmt, ...);
void warnx(const char *fmt, ...);

extern char *progname;

usec microseconds(void);
void init_timeval(struct timeval *tv, usec t);

void ms_init(ms a, ms_event_fn oninsert, ms_event_fn onremove);
void ms_clear(ms a);
int ms_append(ms a, void *item);
int ms_remove(ms a, void *item);
int ms_contains(ms a, void *item);
void *ms_take(ms a);

/* initialize a priority queue */
void pq_init(pq q, job_cmp_fn cmp);

void pq_clear(pq q);

/* return 1 if the job was inserted, else 0 */
int pq_give(pq q, job j);

/* return a job if the queue contains jobs, else NULL */
job pq_take(pq q);

/* return a job if the queue contains jobs, else NULL */
job pq_peek(pq q);

/* remove and return j if the queue contains j, else return NULL */
job pq_remove(pq q, job j);


#define make_job(pri,delay,ttr,body_size,tube) make_job_with_id(pri,delay,ttr,body_size,tube,0)

job allocate_job(int body_size);
job make_job_with_id(uint pri, usec delay, usec ttr,
             int body_size, tube tube, uint64 id);
void job_free(job j);

/* Lookup a job by job ID */
job job_find(uint64 job_id);

int job_pri_cmp(job a, job b);
int job_delay_cmp(job a, job b);

job job_copy(job j);

const char * job_state(job j);

int job_list_any_p(job head);
job job_remove(job j);
void job_insert(job head, job j);

uint64 total_jobs();

/* for unit tests */
size_t get_all_jobs_used();

void job_init();


extern struct ms tubes;

tube make_tube(const char *name);
void tube_dref(tube t);
void tube_iref(tube t);
tube tube_find(const char *name);
tube tube_find_or_make(const char *name);
#define TUBE_ASSIGN(a,b) (tube_dref(a), (a) = (b), tube_iref(a))


conn make_conn(int fd, char start_state, tube use, tube watch);

int conn_set_evq(conn c, const int events, evh handler);
void conn_set_evmask(conn c, const int evmask, conn list);
int conn_update_net(conn c);

void conn_close(conn c);

conn conn_remove(conn c);
void conn_insert(conn head, conn c);

int count_cur_conns();
uint count_tot_conns();
int count_cur_producers();
int count_cur_workers();

void conn_set_producer(conn c);
void conn_set_worker(conn c);

job soonest_job(conn c);
int has_reserved_this_job(conn c, job j);
int conn_has_close_deadline(conn c);
int conn_ready(conn c);

#define conn_waiting(c) ((c)->type & CONN_TYPE_WAITING)


extern size_t primes[];


extern size_t job_data_size_limit;

void prot_init();

conn remove_waiting_conn(conn c);

void enqueue_reserved_jobs(conn c);

void enter_drain_mode(int sig);
void h_accept(const int fd, const short which, struct event *ev);
void prot_remove_tube(tube t);
void prot_replay_binlog(job binlog_jobs);


int make_server_socket(char *host_addr, char *port);

void unbrake();
extern int listening;
extern evh accept_handler;


extern char *binlog_dir;
extern size_t binlog_size_limit;
#define BINLOG_SIZE_LIMIT_DEFAULT (10 << 20)

extern int enable_fsync;
extern size_t fsync_throttle_ms;

void binlog_init(job binlog_jobs);

/* Return the number of locks acquired: either 0 or 1. */
int binlog_lock();

/* Returns the number of jobs successfully written (either 0 or 1). */
int binlog_write_job(job j);
size_t binlog_reserve_space_put(job j);
size_t binlog_reserve_space_update(job j);

void binlog_shutdown();
const char *binlog_oldest_index();
const char *binlog_current_index();
