typedef unsigned char uchar;
typedef uchar         byte;
typedef unsigned int  uint;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;

#define int8_t   do_not_use_int8_t
#define uint8_t  do_not_use_uint8_t
#define int32_t  do_not_use_int32_t
#define uint32_t do_not_use_uint32_t
#define int64_t  do_not_use_int64_t
#define uint64_t do_not_use_uint64_t

typedef struct ms     *ms;
typedef struct job    *job;
typedef struct tube   *tube;
typedef struct conn   *conn;
typedef struct Heap   Heap;
typedef struct Jobrec Jobrec;
typedef struct File   File;
typedef struct Socket Socket;
typedef struct Srv    Srv;
typedef struct Wal    Wal;

typedef void(*evh)(int, short, void *);
typedef void(*ms_event_fn)(ms a, void *item, size_t i);
typedef void(*Handle)(void*, int);
typedef int(*Less)(void*, void*);
typedef void(*Record)(void*, int);

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

/* CONN_TYPE_* are bit masks */
#define CONN_TYPE_PRODUCER 1
#define CONN_TYPE_WORKER   2
#define CONN_TYPE_WAITING  4

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

extern const char version[];

struct stats {
    uint urgent_ct;
    uint waiting_ct;
    uint buried_ct;
    uint reserved_ct;
    uint pause_ct;
    uint64   total_jobs_ct;
};


struct Heap {
    int     cap;
    int     len;
    void    **data;
    Less    less;
    Record  rec;
};
int   heapinsert(Heap *h, void *x);
void* heapremove(Heap *h, int k);


struct Socket {
    int    fd;
    Handle f;
    void   *x;
    int    added;
};

void sockinit(Handle tick, void *x, int64 ns);
int  sockwant(Socket *s, int rw);
void sockmain(void); // does not return

struct ms {
    size_t used, cap, last;
    void **items;
    ms_event_fn oninsert, onremove;
};

enum
{
    Walver = 7
};

enum // Jobrec.state
{
    Invalid,
    Ready,
    Reserved,
    Buried,
    Delayed,
    Copy
};

// if you modify this struct, you must increment Walver above
struct Jobrec {
    uint64 id;
    uint32 pri;
    int64  delay;
    int64  ttr;
    int32  body_size;
    int64  created_at;
    int64  deadline_at;
    uint32 reserve_ct;
    uint32 timeout_ct;
    uint32 release_ct;
    uint32 bury_ct;
    uint32 kick_ct;
    byte   state;
};

struct job {
    Jobrec r; // persistent fields; these get written to the wal

    /* bookeeping fields; these are in-memory only */
    char pad[6];
    tube tube;
    job prev, next; /* linked list of jobs */
    job ht_next; /* Next job in a hash table list */
    size_t heap_index; /* where is this job in its current heap */
    File *file;
    job  fnext;
    job  fprev;
    void *reserver;
    int walresv;
    int walused;

    char body[]; // written separately to the wal
};

struct tube {
    uint refs;
    char name[MAX_TUBE_NAME_LEN];
    Heap ready;
    Heap delay;
    struct ms waiting; /* set of conns */
    struct stats stat;
    uint using_ct;
    uint watching_ct;
    int64 pause;
    int64 deadline_at;
    struct job buried;
};

struct conn {
    conn prev, next; /* linked list of connections */
    Srv *srv;
    Socket sock;
    char state;
    char type;
    int rw; // currently want: 'r' or 'w'
    int pending_timeout;
    int64 tickat; // time at which to do more work
    int tickpos; // position in srv->conns

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
    tube use;
    struct ms watch;
    struct job reserved_jobs; /* doubly-linked list header */
};


void v(void);

void warn(const char *fmt, ...);
void warnx(const char *fmt, ...);
char* fmtalloc(char *fmt, ...);
void* zalloc(int n);
#define new(T) zalloc(sizeof(T))

extern const char *progname;

int64 nanoseconds(void);
int   falloc(int fd, int len);


void ms_init(ms a, ms_event_fn oninsert, ms_event_fn onremove);
void ms_clear(ms a);
int ms_append(ms a, void *item);
int ms_remove(ms a, void *item);
int ms_contains(ms a, void *item);
void *ms_take(ms a);


#define make_job(pri,delay,ttr,body_size,tube) make_job_with_id(pri,delay,ttr,body_size,tube,0)

job allocate_job(int body_size);
job make_job_with_id(uint pri, int64 delay, int64 ttr,
             int body_size, tube tube, uint64 id);
void job_free(job j);

/* Lookup a job by job ID */
job job_find(uint64 job_id);

/* the void* parameters are really job pointers */
void job_setheappos(void*, int);
int job_pri_less(void*, void*);
int job_delay_less(void*, void*);

job job_copy(job j);

const char * job_state(job j);

int job_list_any_p(job head);
job job_remove(job j);
void job_insert(job head, job j);

uint64 total_jobs(void);

/* for unit tests */
size_t get_all_jobs_used(void);


extern struct ms tubes;

tube make_tube(const char *name);
void tube_dref(tube t);
void tube_iref(tube t);
tube tube_find(const char *name);
tube tube_find_or_make(const char *name);
#define TUBE_ASSIGN(a,b) (tube_dref(a), (a) = (b), tube_iref(a))


conn make_conn(int fd, char start_state, tube use, tube watch);

int  connless(conn a, conn b);
void connrec(conn c, int i);
void connwant(conn c, const int mask, conn list);
void connsched(conn c);

void conn_close(conn c);

conn conn_remove(conn c);
void conn_insert(conn head, conn c);

int count_cur_conns(void);
uint count_tot_conns(void);
int count_cur_producers(void);
int count_cur_workers(void);

void conn_set_producer(conn c);
void conn_set_worker(conn c);

job soonest_job(conn c);
int has_reserved_this_job(conn c, job j);
int conn_has_close_deadline(conn c);
int conn_ready(conn c);

#define conn_waiting(c) ((c)->type & CONN_TYPE_WAITING)


extern size_t primes[];


extern size_t job_data_size_limit;

void prot_init(void);
void prottick(Srv *s);

conn remove_waiting_conn(conn c);

void enqueue_reserved_jobs(conn c);

void enter_drain_mode(int sig);
void h_accept(const int fd, const short which, Srv* srv);
void prot_remove_tube(tube t);
void prot_replay(Srv *s, job list);


int make_server_socket(char *host_addr, char *port);


enum
{
    Filesizedef = (10 << 20)
};

struct Wal {
    int    use;
    char   *dir;
    File   *head;
    File   *cur;
    File   *tail;
    int    nfile;
    int    next;
    int    filesz;
    int    resv;  // bytes reserved
    int    alive; // bytes in use
    int64  nmig;  // migrations
    int64  nrec;  // records written ever
    int    wantsync;
    int64  syncrate;
    int64  lastsync;
    int    nocomp; // disable binlog compaction?
};
int  waldirlock(Wal*);
void walinit(Wal*, job list);
int  walwrite(Wal*, job);
void walmaint(Wal*);
int  walresvput(Wal*, job);
int  walresvupdate(Wal*, job);
void walgc(Wal*);


struct File {
    File *next;
    uint refs;
    int  seq;
    int  iswopen; // is open for writing
    int  fd;
    int  free;
    int  resv;
    char *path;
    Wal  *w;

    struct job jlist; // jobs written in this file
};
int  fileinit(File*, Wal*, int);
Wal* fileadd(File*, Wal*);
void fileincref(File*);
void filedecref(File*);
void fileaddjob(File*, job);
void filermjob(File*, job);
int  fileread(File*, job list);
void filewopen(File*);
void filewclose(File*);
int  filewrjobshort(File*, job);
int  filewrjobfull(File*, job);


struct Srv {
    Socket sock;
    Heap   conns;
    Wal    wal;
};
void srv(Srv *srv);
void srvaccept(Srv *s, int ev);
void srvschedconn(Srv *srv, conn c);
void srvtick(Srv *s, int ev);
