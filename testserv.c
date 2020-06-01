#include "ct/ct.h"
#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

static int srvpid, size;

// Global timeout set for reading response in tests; 5sec.
static int64 timeout = 5000000000LL;

// Allocation pattern for wrapfalloc that replaces falloc in tests.
// Zero value at N-th element means that N-th call to the falloc
// should fail with ENOSPC result.
static byte fallocpat[3];


static int
exist(char *path)
{
    struct stat s;

    int r = stat(path, &s);
    return r != -1;
}

static int
wrapfalloc(int fd, int size)
{
    static size_t c = 0;

    printf("\nwrapfalloc: fd=%d size=%d\n", fd, size);
    if (c >= sizeof(fallocpat) || !fallocpat[c++]) {
        return ENOSPC;
    }
    return rawfalloc(fd, size);
}

static void
muststart(char *a0, char *a1, char *a2, char *a3, char *a4)
{
    srvpid = fork();
    if (srvpid < 0) {
        twarn("fork");
        exit(1);
    }

    if (srvpid > 0) {
        printf("%s %s %s %s %s\n", a0, a1, a2, a3, a4);
        printf("start server pid=%d\n", srvpid);
        usleep(100000); // .1s; time for the child to bind to its port
        return;
    }

    /* now in child */

    execlp(a0, a0, a1, a2, a3, a4, NULL);
}

static int
mustdiallocal(int port)
{
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };

    int r = inet_aton("127.0.0.1", &addr.sin_addr);
    if (!r) {
        errno = EINVAL;
        twarn("inet_aton");
        exit(1);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        twarn("socket");
        exit(1);
    }

    // Fix of the benchmarking issue on Linux. See issue #430.
    int flags = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(int))) {
        twarn("setting TCP_NODELAY on fd %d", fd);
        exit(1);
    }

    r = connect(fd, (struct sockaddr *)&addr, sizeof addr);
    if (r == -1) {
        twarn("connect");
        exit(1);
    }

    return fd;
}

static int
mustdialunix(char *socket_file)
{
    struct sockaddr_un addr;
    const size_t maxlen = sizeof(addr.sun_path);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, maxlen, "%s", socket_file);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        twarn("socket");
        exit(1);
    }

    int r = connect(fd, (struct sockaddr *)&addr, sizeof addr);
    if (r == -1) {
        twarn("connect");
        exit(1);
    }

    return fd;
}

static void
exit_process(int signum)
{
    UNUSED_PARAMETER(signum);
    exit(0);
}

static void
set_sig_handler()
{
    struct sigaction sa;

    sa.sa_flags = 0;
    int r = sigemptyset(&sa.sa_mask);
    if (r == -1) {
        twarn("sigemptyset()");
        exit(111);
    }

    // This is required to trigger gcov on exit. See issue #443.
    sa.sa_handler = exit_process;
    r = sigaction(SIGTERM, &sa, 0);
    if (r == -1) {
        twarn("sigaction(SIGTERM)");
        exit(111);
    }
}

// Kill the srvpid (child process) with SIGTERM to give it a chance
// to write gcov data to the filesystem before ct kills it with SIGKILL.
// Do nothing in case of srvpid==0; child was already killed.
static void
kill_srvpid(void)
{
    if (!srvpid)
        return;
    kill(srvpid, SIGTERM);
    waitpid(srvpid, 0, 0);
    srvpid = 0;
}

#define SERVER() (progname=__func__, mustforksrv())
#define SERVER_UNIX() (progname=__func__, mustforksrv_unix())

// Forks the server storing the pid in srvpid.
// The parent process returns port assigned.
// The child process serves until the SIGTERM is received by it.
static int
mustforksrv(void)
{
    struct sockaddr_in addr;

    srv.sock.fd = make_server_socket("127.0.0.1", "0");
    if (srv.sock.fd == -1) {
        puts("mustforksrv failed");
        exit(1);
    }

    size_t len = sizeof(addr);
    int r = getsockname(srv.sock.fd, (struct sockaddr *)&addr, (socklen_t *)&len);
    if (r == -1 || len > sizeof(addr)) {
        puts("mustforksrv failed");
        exit(1);
    }

    int port = ntohs(addr.sin_port);
    srvpid = fork();
    if (srvpid < 0) {
        twarn("fork");
        exit(1);
    }

    if (srvpid > 0) {
        // On exit the parent (test) sends SIGTERM to the child.
        atexit(kill_srvpid);
        printf("start server port=%d pid=%d\n", port, srvpid);
        return port;
    }

    /* now in child */

    set_sig_handler();
    prot_init();

    srv_acquire_wal(&srv);

    srvserve(&srv); /* does not return */
    exit(1); /* satisfy the compiler */
}

static char *
mustforksrv_unix(void)
{
    static char path[90];
    char name[95];
    snprintf(path, sizeof(path), "%s/socket", ctdir());
    snprintf(name, sizeof(name), "unix:%s", path);
    srv.sock.fd = make_server_socket(name, NULL);
    if (srv.sock.fd == -1) {
        puts("mustforksrv_unix failed");
        exit(1);
    }

    srvpid = fork();
    if (srvpid < 0) {
        twarn("fork");
        exit(1);
    }

    if (srvpid > 0) {
        // On exit the parent (test) sends SIGTERM to the child.
        atexit(kill_srvpid);
        printf("start server socket=%s\n", path);
        assert(exist(path));
        return path;
    }

    /* now in child */

    set_sig_handler();
    prot_init();

    srv_acquire_wal(&srv);

    srvserve(&srv); /* does not return */
    exit(1); /* satisfy the compiler */
}

static char *
readline(int fd)
{
    char c = 0, p = 0;
    static char buf[1024];
    fd_set rfd;
    struct timeval tv;

    printf("<%d ", fd);
    fflush(stdout);

    size_t i = 0;
    for (;;) {
        FD_ZERO(&rfd);
        FD_SET(fd, &rfd);
        tv.tv_sec = timeout / 1000000000;
        tv.tv_usec = (timeout/1000) % 1000000;
        int r = select(fd+1, &rfd, NULL, NULL, &tv);
        switch (r) {
        case 1:
            break;
        case 0:
            fputs("timeout", stderr);
            exit(8);
        case -1:
            perror("select");
            exit(1);
        default:
            fputs("unknown error", stderr);
            exit(3);
        }

        // TODO: try reading into a buffer to improve performance.
        // See related issue #430.
        r = read(fd, &c, 1);
        if (r == -1) {
            perror("write");
            exit(1);
        }
        if (i >= sizeof(buf)-1) {
            fputs("response too big", stderr);
            exit(4);
        }
        putc(c, stdout);
        fflush(stdout);
        buf[i++] = c;
        if (p == '\r' && c == '\n') {
            break;
        }
        p = c;
    }
    buf[i] = '\0';
    return buf;
}

static void
ckresp(int fd, char *exp)
{
    char *line = readline(fd);
    assertf(strcmp(exp, line) == 0, "\"%s\" != \"%s\"", exp, line);
}

static void
ckrespsub(int fd, char *sub)
{
    char *line = readline(fd);
    assertf(strstr(line, sub), "\"%s\" not in \"%s\"", sub, line);
}

static void
writefull(int fd, char *s, int n)
{
    int c;
    for (; n; n -= c) {
        c = write(fd, s, n);
        if (c == -1) {
            perror("write");
            exit(1);
        }
        s += c;
    }
}

static void
mustsend(int fd, char *s)
{
    writefull(fd, s, strlen(s));
    printf(">%d %s", fd, s);
    fflush(stdout);
}

static int
filesize(char *path)
{
    struct stat s;

    int r = stat(path, &s);
    if (r == -1) {
        twarn("stat");
        exit(1);
    }
    return s.st_size;
}

void
cttest_unknown_command()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "nont10knowncommand\r\n");
    ckresp(fd, "UNKNOWN_COMMAND\r\n");
}

void
cttest_too_long_commandline()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    int i;
    for (i = 0; i < 10; i++)
        mustsend(fd, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); // 50 bytes
    mustsend(fd, "\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    // Issue another command and check that reponse is not "UNKNOWN_COMMAND"
    // as described in issue #337
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}

void
cttest_put_in_drain()
{
    enter_drain_mode(SIGUSR1);
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "x\r\n");
    ckresp(fd, "DRAINING\r\n");
}

void
cttest_peek_ok()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "peek 1\r\n");
    ckresp(fd, "FOUND 1 1\r\n");
    ckresp(fd, "a\r\n");
}

void
cttest_peek_not_found()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "peek 2\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
    mustsend(fd, "peek 18446744073709551615\r\n");  // UINT64_MAX
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_peek_ok_unix()
{
    char *name = SERVER_UNIX();
    int fd = mustdialunix(name);
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "peek 1\r\n");
    ckresp(fd, "FOUND 1 1\r\n");
    ckresp(fd, "a\r\n");

    unlink(name);
}

void
cttest_unix_auto_removal()
{
    // Twice, to trigger autoremoval
    SERVER_UNIX();
    kill_srvpid();
    SERVER_UNIX();
}

void
cttest_peek_bad_format()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "peek 18446744073709551616\r\n"); // UINT64_MAX+1
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "peek 184467440737095516160000000000000000000000000000\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "peek foo111\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "peek 111foo\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_peek_delayed()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "peek-delayed\r\n");
    ckresp(fd, "NOT_FOUND\r\n");

    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 99 1 1\r\n");
    mustsend(fd, "B\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "put 0 1 1 1\r\n");
    mustsend(fd, "C\r\n");
    ckresp(fd, "INSERTED 3\r\n");

    mustsend(fd, "peek-delayed\r\n");
    ckresp(fd, "FOUND 3 1\r\n");
    ckresp(fd, "C\r\n");

    mustsend(fd, "delete 3\r\n");
    ckresp(fd, "DELETED\r\n");

    mustsend(fd, "peek-delayed\r\n");
    ckresp(fd, "FOUND 2 1\r\n");
    ckresp(fd, "B\r\n");

    mustsend(fd, "delete 2\r\n");
    ckresp(fd, "DELETED\r\n");

    mustsend(fd, "peek-delayed\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_peek_buried_kick()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    // cannot bury unreserved job
    mustsend(fd, "bury 1 0\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
    mustsend(fd, "peek-buried\r\n");
    ckresp(fd, "NOT_FOUND\r\n");

    mustsend(fd, "reserve-with-timeout 0\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "A\r\n");

    // now we can bury
    mustsend(fd, "bury 1 0\r\n");
    ckresp(fd, "BURIED\r\n");
    mustsend(fd, "peek-buried\r\n");
    ckresp(fd, "FOUND 1 1\r\n");
    ckresp(fd, "A\r\n");

    // kick and verify the job is ready
    mustsend(fd, "kick 1\r\n");
    ckresp(fd, "KICKED 1\r\n");
    mustsend(fd, "peek-buried\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
    mustsend(fd, "peek-ready\r\n");
    ckresp(fd, "FOUND 1 1\r\n");
    ckresp(fd, "A\r\n");

    // nothing is left to kick
    mustsend(fd, "kick 1\r\n");
    ckresp(fd, "KICKED 0\r\n");
}

void
cttest_touch_bad_format()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "touch a111\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "touch 111a\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "touch !@#!@#\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_touch_not_found()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "touch 1\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
    mustsend(fd, "touch 100000000000000\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_bury_bad_format()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "bury 111abc 2\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "bury 111\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "bury 111 222abc\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_kickjob_bad_format()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "kick-job a111\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "kick-job 111a\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "kick-job !@#!@#\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_kickjob_buried()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "reserve\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "A\r\n");
    mustsend(fd, "bury 1 0\r\n");
    ckresp(fd, "BURIED\r\n");

    mustsend(fd, "kick-job 100\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
    mustsend(fd, "kick-job 1\r\n");
    ckresp(fd, "KICKED\r\n");
    mustsend(fd, "kick-job 1\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_kickjob_delayed()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    // jid=1 - no delay, jid=2 - delay
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 10 1 1\r\n");
    mustsend(fd, "B\r\n");
    ckresp(fd, "INSERTED 2\r\n");

    mustsend(fd, "kick-job 1\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
    mustsend(fd, "kick-job 2\r\n");
    ckresp(fd, "KICKED\r\n");
    mustsend(fd, "kick-job 2\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_pause()
{
    int64 s;

    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 1\r\n");
    mustsend(fd, "x\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    s = nanoseconds();
    mustsend(fd, "pause-tube default 1\r\n");
    ckresp(fd, "PAUSED\r\n");
    mustsend(fd, "reserve\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "x\r\n");
    assert(nanoseconds() - s >= 1000000000); // 1s
}

void
cttest_underscore()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "use x_y\r\n");
    ckresp(fd, "USING x_y\r\n");
}

void
cttest_2cmdpacket()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "use a\r\nuse b\r\n");
    ckresp(fd, "USING a\r\n");
    ckresp(fd, "USING b\r\n");
}

void
cttest_too_big()
{
    job_data_size_limit = 10;
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 11\r\n");
    mustsend(fd, "delete 9999\r\n");
    mustsend(fd, "put 0 0 0 1\r\n");
    mustsend(fd, "x\r\n");
    ckresp(fd, "JOB_TOO_BIG\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}

void
cttest_job_size_invalid()
{
    job_data_size_limit = JOB_DATA_SIZE_LIMIT_MAX;
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 4294967296\r\n");
    mustsend(fd, "put 0 0 0 10b\r\n");
    mustsend(fd, "put 0 0 0 --!@#$%^&&**()0b\r\n");
    mustsend(fd, "put 0 0 0 1\r\n");
    mustsend(fd, "x\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}

void
cttest_job_size_max_plus_1()
{
    /* verify that server reject the job larger than maximum allowed. */
    job_data_size_limit = JOB_DATA_SIZE_LIMIT_MAX;
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 1073741825\r\n");

    const int len = 1024*1024;
    char body[len+1];
    memset(body, 'a', len);
    body[len] = 0;

    int i;
    for (i=0; i<JOB_DATA_SIZE_LIMIT_MAX; i+=len) {
        mustsend(fd, body);
    }
    mustsend(fd, "x");
    mustsend(fd, "\r\n");
    ckresp(fd, "JOB_TOO_BIG\r\n");
}

void
cttest_delete_ready()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");
}

void
cttest_delete_reserved_by_other()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    int o = mustdiallocal(port);
    mustsend(o, "reserve\r\n");
    ckresp(o, "RESERVED 1 1\r\n");
    ckresp(o, "a\r\n");

    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_delete_bad_format()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "delete 18446744073709551616\r\n"); // UINT64_MAX+1
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "delete 184467440737095516160000000000000000000000000000\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "delete foo111\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "delete 111foo\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_multi_tube()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "use abc\r\n");
    ckresp(fd, "USING abc\r\n");
    mustsend(fd, "put 999999 0 0 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "use def\r\n");
    ckresp(fd, "USING def\r\n");
    mustsend(fd, "put 99 0 0 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "watch abc\r\n");
    ckresp(fd, "WATCHING 2\r\n");
    mustsend(fd, "watch def\r\n");
    ckresp(fd, "WATCHING 3\r\n");
    mustsend(fd, "reserve\r\n");
    ckresp(fd, "RESERVED 2 0\r\n");
}

void
cttest_negative_delay()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 512 -1 100 0\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

/* TODO: add more edge cases tests for delay and ttr */

void
cttest_garbage_priority()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put -1kkdj9djjkd9 0 100 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_negative_priority()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put -1 0 100 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_max_priority()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 4294967295 0 100 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}

void
cttest_too_big_priority()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 4294967296 0 100 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_omit_time_left()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 5 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntime-left: 0\n");
}

void
cttest_small_delay()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 1 1 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}

void
cttest_delayed_to_ready()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 1 1 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-ready: 0\n");

    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-delayed: 1\n");

    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntotal-jobs: 1\n");

    usleep(1010000); // 1.01 sec

    // check that after 1 sec the delayed job is ready again

    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-ready: 1\n");

    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-delayed: 0\n");

    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntotal-jobs: 1\n");
}

void
cttest_statsjob_ck_format()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "stats-job 111ABC\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "stats-job 111 222\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "stats-job 111\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_stats_tube()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "use tubea\r\n");
    ckresp(fd, "USING tubea\r\n");
    mustsend(fd, "put 0 0 0 1\r\n");
    mustsend(fd, "x\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");

    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nname: tubea\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-urgent: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-ready: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-reserved: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-delayed: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-buried: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntotal-jobs: 1\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-using: 1\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-watching: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-waiting: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncmd-delete: 1\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncmd-pause-tube: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npause: 0\n");
    mustsend(fd, "stats-tube tubea\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npause-time-left: 0\n");

    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nname: default\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-urgent: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-ready: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-reserved: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-delayed: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-jobs-buried: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntotal-jobs: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-using: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-watching: 1\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncurrent-waiting: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncmd-delete: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ncmd-pause-tube: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npause: 0\n");
    mustsend(fd, "stats-tube default\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npause-time-left: 0\n");
}

void
cttest_ttrlarge()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 120 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 0 4294 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "put 0 0 4295 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 3\r\n");
    mustsend(fd, "put 0 0 4296 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 4\r\n");
    mustsend(fd, "put 0 0 4297 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 5\r\n");
    mustsend(fd, "put 0 0 5000 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 6\r\n");
    mustsend(fd, "put 0 0 21600 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 7\r\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 120\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 4294\n");
    mustsend(fd, "stats-job 3\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 4295\n");
    mustsend(fd, "stats-job 4\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 4296\n");
    mustsend(fd, "stats-job 5\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 4297\n");
    mustsend(fd, "stats-job 6\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 5000\n");
    mustsend(fd, "stats-job 7\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 21600\n");
}

void
cttest_ttr_small()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 1\n");
}

void
cttest_zero_delay()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}

void
cttest_reserve_with_timeout_2conns()
{
    int fd0, fd1;

    job_data_size_limit = 10;

    int port = SERVER();
    fd0 = mustdiallocal(port);
    fd1 = mustdiallocal(port);
    mustsend(fd0, "watch foo\r\n");
    ckresp(fd0, "WATCHING 2\r\n");
    mustsend(fd0, "reserve-with-timeout 1\r\n");
    mustsend(fd1, "watch foo\r\n");
    ckresp(fd1, "WATCHING 2\r\n");
    timeout = 1100000000; // 1.1s
    ckresp(fd0, "TIMED_OUT\r\n");
}

void
cttest_reserve_ttr_deadline_soon()
{
    int port = SERVER();
    int fd = mustdiallocal(port);

    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "reserve-with-timeout 1\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "a\r\n");

    // After 0.2s the job should be still reserved.
    usleep(200000);
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: reserved\n");

    mustsend(fd, "reserve-with-timeout 1\r\n");
    ckresp(fd, "DEADLINE_SOON\r\n");

    // Job should be reserved; last "reserve" took less than 1s.
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: reserved\n");

    // We don't want to process the job, so release it and check that it's ready.
    mustsend(fd, "release 1 0 0\r\n");
    ckresp(fd, "RELEASED\r\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: ready\n");
}

void
cttest_reserve_job_ttr_deadline_soon()
{
    int port = SERVER();
    int fd = mustdiallocal(port);

    mustsend(fd, "put 0 5 1 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: delayed\n");

    mustsend(fd, "reserve-job 1\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "a\r\n");

    // After 0.1s the job should be still reserved.
    usleep(100000);
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: reserved\n");

    // Reservation made with reserve-job should behave the same way as other
    // reserve commands, e.g. produce "deadline soon" message, and get released
    // when ttr ends.
    mustsend(fd, "reserve-with-timeout 1\r\n");
    ckresp(fd, "DEADLINE_SOON\r\n");

    // Job should be reserved; last "reserve" took less than 1s.
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: reserved\n");

    // We are not able to process the job in time. Check that it gets released.
    // The job was in delayed state. It becomes ready when it gets auto-released.
    usleep(1000000); // 1.0s
    // put a dummy job
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "B\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    // check that ID=1 gets released
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: ready\n");
}

void
cttest_reserve_job_already_reserved()
{
    int port = SERVER();
    int fd = mustdiallocal(port);

    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    mustsend(fd, "reserve-job 1\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "A\r\n");

    // Job should not be reserved twice.
    mustsend(fd, "reserve-job 1\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_reserve_job_ready()
{
    int port = SERVER();
    int fd = mustdiallocal(port);

    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "B\r\n");
    ckresp(fd, "INSERTED 2\r\n");

    mustsend(fd, "reserve-job 2\r\n");
    ckresp(fd, "RESERVED 2 1\r\n");
    ckresp(fd, "B\r\n");

    // Non-existing job.
    mustsend(fd, "reserve-job 3\r\n");
    ckresp(fd, "NOT_FOUND\r\n");

    // id=1 was not reserved.
    mustsend(fd, "release 1 1 0\r\n");
    ckresp(fd, "NOT_FOUND\r\n");

    mustsend(fd, "release 2 1 0\r\n");
    ckresp(fd, "RELEASED\r\n");
}

void
cttest_reserve_job_delayed()
{
    int port = SERVER();
    int fd = mustdiallocal(port);

    mustsend(fd, "put 0 100 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 100 1 1\r\n");
    mustsend(fd, "B\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "put 0 100 1 1\r\n");
    mustsend(fd, "C\r\n");
    ckresp(fd, "INSERTED 3\r\n");

    mustsend(fd, "reserve-job 2\r\n");
    ckresp(fd, "RESERVED 2 1\r\n");
    ckresp(fd, "B\r\n");

    mustsend(fd, "release 2 1 0\r\n");
    ckresp(fd, "RELEASED\r\n");

    // verify that job was released in ready state.
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: ready\n");
}

void
cttest_reserve_job_buried()
{
    int port = SERVER();
    int fd = mustdiallocal(port);

    // put, reserve and bury
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "A\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "reserve-job 1\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "A\r\n");
    mustsend(fd, "bury 1 1\r\n");
    ckresp(fd, "BURIED\r\n");

    // put, reserve and bury
    mustsend(fd, "put 0 0 1 1\r\n");
    mustsend(fd, "B\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "reserve-job 2\r\n");
    ckresp(fd, "RESERVED 2 1\r\n");
    ckresp(fd, "B\r\n");
    mustsend(fd, "bury 2 1\r\n");
    ckresp(fd, "BURIED\r\n");

    // reserve by ids
    mustsend(fd, "reserve-job 2\r\n");
    ckresp(fd, "RESERVED 2 1\r\n");
    ckresp(fd, "B\r\n");
    mustsend(fd, "reserve-job 1\r\n");
    ckresp(fd, "RESERVED 1 1\r\n");
    ckresp(fd, "A\r\n");

    // release back and check if jobs are ready.
    mustsend(fd, "release 1 1 0\r\n");
    ckresp(fd, "RELEASED\r\n");
    mustsend(fd, "release 2 1 0\r\n");
    ckresp(fd, "RELEASED\r\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: ready\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: ready\n");

}

void
cttest_release_bad_format()
{
    int port = SERVER();
    int fd = mustdiallocal(port);

    // bad id
    mustsend(fd, "release 18446744073709551616 1 1\r\n"); // UINT64_MAX+1
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "release 184467440737095516160000000000000000000000000000 1 1\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "release foo111\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
    mustsend(fd, "release 111foo\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");

    // bad priority
    mustsend(fd, "release 18446744073709551615 abc 1\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");

    // bad duration
    mustsend(fd, "release 18446744073709551615 1 abc\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}

void
cttest_release_not_found()
{
    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "release 1 1 1\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_close_releases_job()
{
    int port = SERVER();
    int cons = mustdiallocal(port);
    int prod = mustdiallocal(port);
    mustsend(cons, "reserve-with-timeout 1\r\n");

    mustsend(prod, "put 0 0 100 1\r\n");
    mustsend(prod, "a\r\n");
    ckresp(prod, "INSERTED 1\r\n");

    ckresp(cons, "RESERVED 1 1\r\n");
    ckresp(cons, "a\r\n");

    mustsend(prod, "stats-job 1\r\n");
    ckrespsub(prod, "OK ");
    ckrespsub(prod, "\nstate: reserved\n");

    // Closed consumer connection should make the job ready sooner than ttr=100.
    close(cons);

    // Job should be released in less than 1s. It is not instantly;
    // we do not make guarantees about how soon jobs should be released.
    mustsend(prod, "reserve-with-timeout 1\r\n");
    ckresp(prod, "RESERVED 1 1\r\n");
    ckresp(prod, "a\r\n");
}

void
cttest_quit_releases_job()
{
    // This test is similar to the close_releases_job test, except that
    // connection is not closed, but command quit is sent.
    int port = SERVER();
    int cons = mustdiallocal(port);
    int prod = mustdiallocal(port);
    mustsend(cons, "reserve-with-timeout 1\r\n");

    mustsend(prod, "put 0 0 100 1\r\n");
    mustsend(prod, "a\r\n");
    ckresp(prod, "INSERTED 1\r\n");

    ckresp(cons, "RESERVED 1 1\r\n");
    ckresp(cons, "a\r\n");

    mustsend(prod, "stats-job 1\r\n");
    ckrespsub(prod, "OK ");
    ckrespsub(prod, "\nstate: reserved\n");

    // Quitting consumer should make the job ready sooner than ttr=100.
    mustsend(cons, "quit\r\n");

    // Job should be released in less than 1s. It is not instantly;
    // we do not make guarantees about how soon jobs should be released.
    mustsend(prod, "reserve-with-timeout 1\r\n");
    ckresp(prod, "RESERVED 1 1\r\n");
    ckresp(prod, "a\r\n");
}

void
cttest_unpause_tube()
{
    int fd0, fd1;

    int port = SERVER();
    fd0 = mustdiallocal(port);
    fd1 = mustdiallocal(port);

    mustsend(fd0, "put 0 0 0 0\r\n");
    mustsend(fd0, "\r\n");
    ckresp(fd0, "INSERTED 1\r\n");

    mustsend(fd0, "pause-tube default 86400\r\n");
    ckresp(fd0, "PAUSED\r\n");

    mustsend(fd1, "reserve\r\n");

    mustsend(fd0, "pause-tube default 0\r\n");
    ckresp(fd0, "PAUSED\r\n");

    // ckresp will time out if this takes too long, so the
    // test will not pass.
    ckresp(fd1, "RESERVED 1 0\r\n");
    ckresp(fd1, "\r\n");
}

void
cttest_list_tube()
{
    int port = SERVER();
    int fd0 = mustdiallocal(port);

    mustsend(fd0, "watch w\r\n");
    ckresp(fd0, "WATCHING 2\r\n");

    mustsend(fd0, "use u\r\n");
    ckresp(fd0, "USING u\r\n");

    mustsend(fd0, "list-tubes\r\n");
    ckrespsub(fd0, "OK ");
    ckresp(fd0,
           "---\n"
           "- default\n"
           "- w\n"
           "- u\n\r\n");

    mustsend(fd0, "list-tube-used\r\n");
    ckresp(fd0, "USING u\r\n");

    mustsend(fd0, "list-tubes-watched\r\n");
    ckrespsub(fd0, "OK ");
    ckresp(fd0,
           "---\n"
           "- default\n"
           "- w\n\r\n");

    mustsend(fd0, "ignore default\r\n");
    ckresp(fd0, "WATCHING 1\r\n");

    mustsend(fd0, "list-tubes-watched\r\n");
    ckrespsub(fd0, "OK ");
    ckresp(fd0,
           "---\n"
           "- w\n\r\n");

    mustsend(fd0, "ignore w\r\n");
    ckresp(fd0, "NOT_IGNORED\r\n");
}

#define STRING_LEN_200  \
    "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789" \
    "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"

void
cttest_use_tube_long()
{
    int port = SERVER();
    int fd0 = mustdiallocal(port);
    // 200 chars is okay
    mustsend(fd0, "use " STRING_LEN_200 "\r\n");
    ckresp(fd0, "USING " STRING_LEN_200 "\r\n");
    // 201 chars is too much
    mustsend(fd0, "use " STRING_LEN_200 "Z\r\n");
    ckresp(fd0, "BAD_FORMAT\r\n");
}

void
cttest_longest_command()
{
    int port = SERVER();
    int fd0 = mustdiallocal(port);
    mustsend(fd0, "use " STRING_LEN_200 "\r\n");
    ckresp(fd0, "USING " STRING_LEN_200 "\r\n");
    mustsend(fd0, "pause-tube " STRING_LEN_200 " 4294967295\r\n");
    ckresp(fd0, "PAUSED\r\n");
}

void
cttest_binlog_empty_exit()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    job_data_size_limit = 10;

    int port = SERVER();
    kill_srvpid();

    port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}

void
cttest_binlog_bury()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    job_data_size_limit = 10;

    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 100 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "reserve\r\n");
    ckresp(fd, "RESERVED 1 0\r\n");
    ckresp(fd, "\r\n");
    mustsend(fd, "bury 1 0\r\n");
    ckresp(fd, "BURIED\r\n");
}

void
cttest_binlog_basic()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    job_data_size_limit = 10;

    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 100 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    kill_srvpid();

    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");
}

void
cttest_binlog_size_limit()
{
    int i = 0;
    int gotsize;

    size = 4096;
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.filesize = size;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    int port = SERVER();
    int fd = mustdiallocal(port);
    char *b2 = fmtalloc("%s/binlog.2", ctdir());
    while (!exist(b2)) {
        char *exp = fmtalloc("INSERTED %d\r\n", ++i);
        mustsend(fd, "put 0 0 100 50\r\n");
        mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
        ckresp(fd, exp);
        free(exp);
    }

    char *b1 = fmtalloc("%s/binlog.1", ctdir());
    gotsize = filesize(b1);
    assertf(gotsize == size, "binlog.1 %d != %d", gotsize, size);
    gotsize = filesize(b2);
    assertf(gotsize == size, "binlog.2 %d != %d", gotsize, size);
    free(b1);
    free(b2);
}

void
cttest_binlog_allocation()
{
    int i = 0;

    size = 601;
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.filesize = size;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    int port = SERVER();
    int fd = mustdiallocal(port);
    for (i = 1; i <= 96; i++) {
        char *exp = fmtalloc("INSERTED %d\r\n", i);
        mustsend(fd, "put 0 0 120 22\r\n");
        mustsend(fd, "job payload xxxxxxxxxx\r\n");
        ckresp(fd, exp);
        free(exp);
    }
    for (i = 1; i <= 96; i++) {
        char *exp = fmtalloc("delete %d\r\n", i);
        mustsend(fd, exp);
        ckresp(fd, "DELETED\r\n");
        free(exp);
    }
}

void
cttest_binlog_read()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "use test\r\n");
    ckresp(fd, "USING test\r\n");
    mustsend(fd, "put 0 0 120 4\r\n");
    mustsend(fd, "test\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 0 120 4\r\n");
    mustsend(fd, "tes1\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "watch test\r\n");
    ckresp(fd, "WATCHING 2\r\n");
    mustsend(fd, "reserve\r\n");
    ckresp(fd, "RESERVED 1 4\r\n");
    ckresp(fd, "test\r\n");
    mustsend(fd, "release 1 1 1\r\n");
    ckresp(fd, "RELEASED\r\n");
    mustsend(fd, "reserve\r\n");
    ckresp(fd, "RESERVED 2 4\r\n");
    ckresp(fd, "tes1\r\n");
    mustsend(fd, "delete 2\r\n");
    ckresp(fd, "DELETED\r\n");

    kill_srvpid();

    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "watch test\r\n");
    ckresp(fd, "WATCHING 2\r\n");
    mustsend(fd, "reserve\r\n");
    ckresp(fd, "RESERVED 1 4\r\n");
    ckresp(fd, "test\r\n");
    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 2\r\n");
    ckresp(fd, "NOT_FOUND\r\n");
}

void
cttest_binlog_disk_full()
{
    size = 1000;
    falloc = &wrapfalloc;
    fallocpat[0] = 1;
    fallocpat[2] = 1;

    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.filesize = size;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 3\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 4\r\n");

    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "OUT_OF_MEMORY\r\n");

    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 6\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 7\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 8\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 9\r\n");

    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 2\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 3\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 4\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 6\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 7\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 8\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 9\r\n");
    ckresp(fd, "DELETED\r\n");
}

void
cttest_binlog_disk_full_delete()
{
    size = 1000;
    falloc = &wrapfalloc;
    fallocpat[0] = 1;
    fallocpat[1] = 1;

    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.filesize = size;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    int port = SERVER();
    int fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 2\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 3\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 4\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 5\r\n");

    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 6\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 7\r\n");
    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "INSERTED 8\r\n");

    mustsend(fd, "put 0 0 100 50\r\n");
    mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
    ckresp(fd, "OUT_OF_MEMORY\r\n");

    char *b1 = fmtalloc("%s/binlog.1", ctdir());
    assert(exist(b1));
    free(b1);

    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 2\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 3\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 4\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 5\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 6\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 7\r\n");
    ckresp(fd, "DELETED\r\n");
    mustsend(fd, "delete 8\r\n");
    ckresp(fd, "DELETED\r\n");
}

void
cttest_binlog_v5()
{
    char portstr[10];

    if (system("which beanstalkd-1.4.6") != 0) {
        puts("beanstalkd 1.4.6 not found, skipping");
        exit(0);
    }

    progname = __func__;
    int port = (rand() & 0xfbff) + 1024;
    sprintf(portstr, "%d", port);
    muststart("beanstalkd-1.4.6", "-b", ctdir(), "-p", portstr);
    int fd = mustdiallocal(port);
    mustsend(fd, "use test\r\n");
    ckresp(fd, "USING test\r\n");
    mustsend(fd, "put 1 2 3 4\r\n");
    mustsend(fd, "test\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "put 4 3 2 1\r\n");
    mustsend(fd, "x\r\n");
    ckresp(fd, "INSERTED 2\r\n");

    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nid: 1\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntube: test\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: delayed\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npri: 1\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ndelay: 2\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 3\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreserves: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntimeouts: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreleases: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nburies: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nkicks: 0\n");

    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nid: 2\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntube: test\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: delayed\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npri: 4\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ndelay: 3\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 2\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreserves: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntimeouts: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreleases: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nburies: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nkicks: 0\n");

    kill(srvpid, SIGTERM);
    waitpid(srvpid, NULL, 0);

    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    port = SERVER();
    fd = mustdiallocal(port);

    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nid: 1\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntube: test\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: delayed\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npri: 1\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ndelay: 2\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 3\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreserves: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntimeouts: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreleases: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nburies: 0\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nkicks: 0\n");

    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nid: 2\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntube: test\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nstate: delayed\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\npri: 4\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ndelay: 3\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 2\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreserves: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntimeouts: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nreleases: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nburies: 0\n");
    mustsend(fd, "stats-job 2\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nkicks: 0\n");
}

static void
bench_put_delete_size(int n, int size, int walsize, int sync, int64 syncrate_ms)
{
    if (walsize > 0) {
        srv.wal.dir = ctdir();
        srv.wal.use = 1;
        srv.wal.filesize = walsize;
        srv.wal.syncrate = syncrate_ms * 1000000;
        srv.wal.wantsync = sync;
    }

    job_data_size_limit = JOB_DATA_SIZE_LIMIT_MAX;
    int port = SERVER();
    int fd = mustdiallocal(port);
    char buf[50], put[50];
    char body[size+1];
    memset(body, 'a', size);
    body[size] = 0;
    ctsetbytes(size);
    sprintf(put, "put 0 0 0 %d\r\n", size);
    ctresettimer();
    int i;
    for (i = 0; i < n; i++) {
        mustsend(fd, put);
        mustsend(fd, body);
        mustsend(fd, "\r\n");
        ckrespsub(fd, "INSERTED ");
        sprintf(buf, "delete %d\r\n", i + 1);
        mustsend(fd, buf);
        ckresp(fd, "DELETED\r\n");
    }
    ctstoptimer();
}

void
ctbench_put_delete_0008(int n)
{
    bench_put_delete_size(n, 8, 0, 0, 0);
}

void
ctbench_put_delete_1024(int n)
{
    bench_put_delete_size(n, 1024, 0, 0, 0);
}

void
ctbench_put_delete_8192(int n)
{
    bench_put_delete_size(n, 8192, 0, 0, 0);
}

void
ctbench_put_delete_81920(int n)
{
    bench_put_delete_size(n, 81920, 0, 0, 0);
}

void
ctbench_put_delete_wal_1024_fsync_000ms(int n)
{
    bench_put_delete_size(n, 1024, 512000, 1, 0);
}

void
ctbench_put_delete_wal_1024_fsync_050ms(int n)
{
    bench_put_delete_size(n, 1024, 512000, 1, 50);
}

void
ctbench_put_delete_wal_1024_fsync_200ms(int n)
{
    bench_put_delete_size(n, 1024, 512000, 1, 200);
}

void
ctbench_put_delete_wal_1024_no_fsync(int n)
{
    bench_put_delete_size(n, 1024, 512000, 0, 0);
}

void
ctbench_put_delete_wal_8192_fsync_000ms(int n)
{
    bench_put_delete_size(n, 8192, 512000, 1, 0);
}

void
ctbench_put_delete_wal_8192_fsync_050ms(int n)
{
    bench_put_delete_size(n, 8192, 512000, 1, 50);
}

void
ctbench_put_delete_wal_8192_fsync_200ms(int n)
{
    bench_put_delete_size(n, 8192, 512000, 1, 200);
}

void
ctbench_put_delete_wal_8192_no_fsync(int n)
{
    bench_put_delete_size(n, 8192, 512000, 0, 0);
}
