#include <stdint.h>
#include <stdlib.h>

typedef unsigned char uchar;
typedef uchar         byte;
typedef unsigned int  uint;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;

/* TODO: typedefs of ms, job and tube should not hide the pointer.
   Make them similar to other typedefs (Conn, Heap).
   Maybem move each typedef next to the corresponding struct definition.
   See issue #458. */
typedef struct ms     *ms;
typedef struct job    *job;
typedef struct tube   *tube;
typedef struct Conn   Conn;
typedef struct Heap   Heap;
typedef struct Jobrec Jobrec;
typedef struct File   File;
typedef struct Socket Socket;
typedef struct Server Server;
typedef struct Wal    Wal;

typedef void(*ms_event_fn)(ms a, void *item, size_t i);
typedef void(*Handle)(void*, int rw);
typedef int(FAlloc)(int, int);


// NUM_PRIMES is used in the jobs hashing.
#if _LP64
#define NUM_PRIMES 48
#else
#define NUM_PRIMES 19
#endif

/* Some compilers (e.g. gcc on SmartOS) define NULL as 0.
 * This is allowed by the C standard, but is unhelpful when
 * using NULL in most pointer contexts with errors turned on. */
#if (defined(sun) || defined(__sun)) && (defined(__SVR4) || defined(__svr4__))
#ifdef NULL
#undef NULL
#endif
#define NULL ((void*)0)
#endif

// The name of a tube cannot be longer than MAX_TUBE_NAME_LEN-1
#define MAX_TUBE_NAME_LEN 201

// A command can be at most LINE_BUF_SIZE chars, including "\r\n". This value
// MUST be enough to hold the longest possible command ("pause-tube a{200} 4294967295\r\n")
// or reply line ("USING a{200}\r\n").
#define LINE_BUF_SIZE 224

// CONN_TYPE_* are bit masks used to track the count of connection types.
#define CONN_TYPE_PRODUCER 1
#define CONN_TYPE_WORKER   2
#define CONN_TYPE_WAITING  4

#define min(a,b) ((a)<(b)?(a):(b))

// Jobs with priority less than URGENT_THRESHOLD are counted as urgent.
#define URGENT_THRESHOLD 1024

// The default maximum job size.
#define JOB_DATA_SIZE_LIMIT_DEFAULT ((1 << 16) - 1)

// The maximum value that job_data_size_limit can be set to via "-z".
// It could be up to INT32_MAX-2 (~2GB), but set it to 1024^3 (1GB).
// The width is restricted by Jobrec.body_size that is int32.
#define JOB_DATA_SIZE_LIMIT_MAX 1073741824

// Maximum value (uint32) allowed in pri, delay and ttr parameters
#define MAX_UINT32 4294967295

// Use this macro to designate unused parameters in functions.
#define UNUSED_PARAMETER(x) (void)(x)

// version is defined in vers.c, see vers.sh for details.
extern const char version[];

// verbose holds the count of -V parameters; it's a verbosity level.
extern int verbose;

extern struct Server srv;

// Replaced by tests to simulate failures.
extern FAlloc *falloc;

// stats structure holds counters for operations, both globally and per tube.
struct stats {
    uint urgent_ct;
    uint waiting_ct;
    uint buried_ct;
    uint reserved_ct;
    uint pause_ct;
    uint64   total_delete_ct;
    uint64   total_jobs_ct;
};


// less_fn is used by the binary heap to determine the order of elements.
typedef int(*less_fn)(void*, void*);

// setpos_fn is used by the binary heap to record the new positions of elements
// whenever they get moved or inserted.
typedef void(*setpos_fn)(void*, size_t);

struct Heap {
    size_t  cap;                // capacity of the heap
    size_t  len;                // amount of elements in the heap
    void    **data;             // actual elements

    less_fn   less;
    setpos_fn setpos;
};
int   heapinsert(Heap *h, void *x);
void* heapremove(Heap *h, size_t k);


struct Socket {
    int    fd;
    Handle f;
    void   *x;
    int    added;
};
int sockinit(void);
int sockwant(Socket*, int);
int socknext(Socket**, int64);

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

// If you modify this struct, you must increment Walver above,
// Beanstalkd does not handle migration between different versions:
// it will reject the old data and exit from the server.
// TODO: Handle Walver migrations automatically.
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
     // persistent fields; these get written to the wal
    Jobrec r;

    // bookeeping fields; these are in-memory only
    char pad[6];
    tube tube;
    job prev, next;             // linked list of jobs
    job ht_next;                // Next job in a hash table list
    size_t heap_index;          // where is this job in its current heap
    File *file;
    job  fnext;
    job  fprev;
    void *reserver;
    int walresv;
    int walused;

    char body[];                // written separately to the wal
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


#define twarn(fmt, args...) warn("%s:%d in %s: " fmt, \
                                 __FILE__, __LINE__, __func__, ##args)
#define twarnx(fmt, args...) warnx("%s:%d in %s: " fmt, \
                                   __FILE__, __LINE__, __func__, ##args)

void warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void warnx(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char* fmtalloc(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void* zalloc(int n);
#define new(T) zalloc(sizeof(T))
void optparse(Server*, char**);

extern const char *progname;

int64 nanoseconds(void);
int   rawfalloc(int fd, int len);


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
void job_setpos(void *j, size_t i);
int job_pri_less(void *ja, void *jb);
int job_delay_less(void *ja, void *jb);

job job_copy(job j);

const char * job_state(job j);

int job_list_any_p(job head);
job job_remove(job j);
void job_insert(job head, job j);

/* for unit tests */
size_t get_all_jobs_used(void);


extern struct ms tubes;

tube make_tube(const char *name);
void tube_dref(tube t);
void tube_iref(tube t);
tube tube_find(const char *name);
tube tube_find_or_make(const char *name);
#define TUBE_ASSIGN(a,b) (tube_dref(a), (a) = (b), tube_iref(a))


Conn *make_conn(int fd, char start_state, tube use, tube watch);

int count_cur_conns(void);
uint count_tot_conns(void);
int count_cur_producers(void);
int count_cur_workers(void);


extern size_t primes[];


extern size_t job_data_size_limit;

void prot_init(void);
int64 prottick(Server *s);

Conn *remove_waiting_conn(Conn *c);

void enqueue_reserved_jobs(Conn *c);

void enter_drain_mode(int sig);
void h_accept(const int fd, const short which, Server* srv);
void prot_remove_tube(tube t);
int  prot_replay(Server *s, job list);


int make_server_socket(char *host_addr, char *port);


struct Conn {
    Server *srv;
    Socket sock;
    char   state;
    char   type;
    Conn   *next;
    tube   use;         // tube currently in use
    int64  tickat;      // time at which to do more work; determines pos in heap
    size_t tickpos;     // position in srv->conns, stale when in_conns=0
    byte   in_conns;    // 1 if the conn is in srv->conns heap, 0 otherwise
    job    soonest_job; // memoization of the soonest job
    int    rw;          // currently want: 'r', 'w', or 'h'
    int    pending_timeout;
    char   halfclosed;

    char   cmd[LINE_BUF_SIZE];  // this string is NOT NUL-terminated
    size_t cmd_len;
    int    cmd_read;

    char *reply;
    int  reply_len;
    int  reply_sent;
    char reply_buf[LINE_BUF_SIZE]; // this string IS NUL-terminated

    // How many bytes of in_job->body have been read so far. If in_job is NULL
    // while in_job_read is nonzero, we are in bit bucket mode and
    // in_job_read's meaning is inverted -- then it counts the bytes that
    // remain to be thrown away.
    int32 in_job_read;
    job   in_job;               // a job to be read from the client

    job out_job;
    int out_job_sent;

    struct ms  watch;
    struct job reserved_jobs; // linked list header
};
int  conn_less(void *ax, void *bx);
void conn_setpos(void *cx, size_t i);
void connwant(Conn *c, int rw);
void connsched(Conn *c);
void connclose(Conn *c);
void connsetproducer(Conn *c);
void connsetworker(Conn *c);
job  connsoonestjob(Conn *c);
int  conndeadlinesoon(Conn *c);
int conn_ready(Conn *c);
#define conn_waiting(c) ((c)->type & CONN_TYPE_WAITING)




enum
{
    Filesizedef = (10 << 20)
};

struct Wal {
    int    filesize;
    int    use;
    char   *dir;
    File   *head;
    File   *cur;
    File   *tail;
    int    nfile;
    int    next;
    int64  resv;  // bytes reserved
    int64  alive; // bytes in use
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
int  walresvupdate(Wal*);
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


#define Portdef "11300"

struct Server {
    char *port;
    char *addr;
    char *user;

    Wal    wal;
    Socket sock;

    // Connections that must produce deadline or timeout, ordered by the time.
    Heap   conns;
};
void srvserve(Server *srv);
void srvaccept(Server *s, int ev);
