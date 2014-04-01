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
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include "ct/ct.h"
#include "dat.h"

static int srvpid, port, fd, size;
static int64 timeout = 5000000000LL; // 5s

static byte fallocpat[3];


static int
wrapfalloc(int fd, int size)
{
    static int c = 0;

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
    int r, fd;
    struct sockaddr_in addr = {};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    r = inet_aton("127.0.0.1", &addr.sin_addr);
    if (!r) {
        errno = EINVAL;
        twarn("inet_aton");
        exit(1);
    }

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        twarn("socket");
        exit(1);
    }

    r = connect(fd, (struct sockaddr*)&addr, sizeof addr);
    if (r == -1) {
        twarn("connect");
        exit(1);
    }

    return fd;
}


#define SERVER() (progname=__func__, mustforksrv())

static int
mustforksrv()
{
    int r, len, port, ok;
    struct sockaddr_in addr;

    srv.sock.fd = make_server_socket("127.0.0.1", "0");
    if (srv.sock.fd == -1) {
        puts("mustforksrv failed");
        exit(1);
    }

    len = sizeof(addr);
    r = getsockname(srv.sock.fd, (struct sockaddr*)&addr, (socklen_t*)&len);
    if (r == -1 || len > sizeof(addr)) {
        puts("mustforksrv failed");
        exit(1);
    }

    port = ntohs(addr.sin_port);
    srvpid = fork();
    if (srvpid < 0) {
        twarn("fork");
        exit(1);
    }

    if (srvpid > 0) {
        printf("start server port=%d pid=%d\n", port, srvpid);
        return port;
    }

    /* now in child */

    prot_init();

    if (srv.wal.use) {
        struct job list = {};
        // We want to make sure that only one beanstalkd tries
        // to use the wal directory at a time. So acquire a lock
        // now and never release it.
        if (!waldirlock(&srv.wal)) {
            twarnx("failed to lock wal dir %s", srv.wal.dir);
            exit(10);
        }

        list.prev = list.next = &list;
        walinit(&srv.wal, &list);
        ok = prot_replay(&srv, &list);
        if (!ok) {
            twarnx("failed to replay log");
            exit(11);
        }
    }

    srvserve(&srv); /* does not return */
    exit(1); /* satisfy the compiler */
}


static char *
readline(int fd)
{
    int r, i = 0;
    char c = 0, p = 0;
    static char buf[1024];
    fd_set rfd;
    struct timeval tv;

    printf("<%d ", fd);
    fflush(stdout);
    for (;;) {
        FD_ZERO(&rfd);
        FD_SET(fd, &rfd);
        tv.tv_sec = timeout / 1000000000;
        tv.tv_usec = (timeout/1000) % 1000000;
        r = select(fd+1, &rfd, NULL, NULL, &tv);
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
    char *line;

    line = readline(fd);
    assertf(strcmp(exp, line) == 0, "\"%s\" != \"%s\"", exp, line);
}


static void
ckrespsub(int fd, char *sub)
{
    char *line;

    line = readline(fd);
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
    int r;
    struct stat s;

    r = stat(path, &s);
    if (r == -1) {
        twarn("stat");
        exit(1);
    }
    return s.st_size;
}


static int
exist(char *path)
{
    int r;
    struct stat s;

    r = stat(path, &s);
    return r != -1;
}


void
cttestpause()
{
    int64 s;

    port = SERVER();
    fd = mustdiallocal(port);
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
cttestunderscore()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "use x_y\r\n");
    ckresp(fd, "USING x_y\r\n");
}


void
cttest2cmdpacket()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "use a\r\nuse b\r\n");
    ckresp(fd, "USING a\r\n");
    ckresp(fd, "USING b\r\n");
}


void
cttesttoobig()
{
    job_data_size_limit = 10;
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 11\r\n");
    mustsend(fd, "delete 9999\r\n");
    mustsend(fd, "put 0 0 0 1\r\n");
    mustsend(fd, "x\r\n");
    ckresp(fd, "JOB_TOO_BIG\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}


void
cttestdeleteready()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");
}


void
cttestmultitube()
{
    port = SERVER();
    fd = mustdiallocal(port);
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
cttestnonegativedelay()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 512 -1 100 0\r\n");
    ckresp(fd, "BAD_FORMAT\r\n");
}


void
cttestomittimeleft()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 5 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\ntime-left: 0\n");
}


void
cttestsmalldelay()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 1 1 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}


void
ctteststatstube()
{
    port = SERVER();
    fd = mustdiallocal(port);
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
cttestttrlarge()
{
    port = SERVER();
    fd = mustdiallocal(port);
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
cttestttrsmall()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 1\r\n");
    mustsend(fd, "a\r\n");
    ckresp(fd, "INSERTED 1\r\n");
    mustsend(fd, "stats-job 1\r\n");
    ckrespsub(fd, "OK ");
    ckrespsub(fd, "\nttr: 1\n");
}


void
cttestzerodelay()
{
    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 1 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}


void
cttestreservewithtimeout2conn()
{
    int fd0, fd1;

    job_data_size_limit = 10;

    port = SERVER();
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
cttestunpausetube()
{
    int fd0, fd1;

    port = SERVER();
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
cttestbinlogemptyexit()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    job_data_size_limit = 10;

    port = SERVER();

    kill(srvpid, 9);
    waitpid(srvpid, NULL, 0);

    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 0 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");
}


void
cttestbinlogbury()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    job_data_size_limit = 10;

    port = SERVER();
    fd = mustdiallocal(port);
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
cttestbinlogbasic()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    job_data_size_limit = 10;

    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "put 0 0 100 0\r\n");
    mustsend(fd, "\r\n");
    ckresp(fd, "INSERTED 1\r\n");

    kill(srvpid, 9);
    waitpid(srvpid, NULL, 0);

    port = SERVER();
    fd = mustdiallocal(port);
    mustsend(fd, "delete 1\r\n");
    ckresp(fd, "DELETED\r\n");
}


void
cttestbinlogsizelimit()
{
    int i = 0;
    char *b2;
    int gotsize;

    size = 1024;
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.filesize = size;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    port = SERVER();
    fd = mustdiallocal(port);
    b2 = fmtalloc("%s/binlog.2", ctdir());
    while (!exist(b2)) {
        mustsend(fd, "put 0 0 100 50\r\n");
        mustsend(fd, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n");
        ckresp(fd, fmtalloc("INSERTED %d\r\n", ++i));
    }

    gotsize = filesize(fmtalloc("%s/binlog.1", ctdir()));
    assertf(gotsize == size, "binlog.1 %d != %d", gotsize, size);
    gotsize = filesize(b2);
    assertf(gotsize == size, "binlog.2 %d != %d", gotsize, size);
}


void
cttestbinlogallocation()
{
    int i = 0;

    size = 601;
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.filesize = size;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    port = SERVER();
    fd = mustdiallocal(port);
    for (i = 1; i <= 96; i++) {
        mustsend(fd, "put 0 0 120 22\r\n");
        mustsend(fd, "job payload xxxxxxxxxx\r\n");
        ckresp(fd, fmtalloc("INSERTED %d\r\n", i));
    }
    for (i = 1; i <= 96; i++) {
        mustsend(fd, fmtalloc("delete %d\r\n", i));
        ckresp(fd, "DELETED\r\n");
    }
}


void
cttestbinlogread()
{
    srv.wal.dir = ctdir();
    srv.wal.use = 1;
    srv.wal.syncrate = 0;
    srv.wal.wantsync = 1;

    port = SERVER();
    fd = mustdiallocal(port);
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

    kill(srvpid, 9);
    waitpid(srvpid, NULL, 0);

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
cttestbinlogdiskfull()
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

    port = SERVER();
    fd = mustdiallocal(port);
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
cttestbinlogdiskfulldelete()
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

    port = SERVER();
    fd = mustdiallocal(port);
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

    assert(exist(fmtalloc("%s/binlog.1", ctdir())));

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
cttestbinlogv5()
{
    char portstr[10];

    if (system("which beanstalkd-1.4.6") != 0) {
        puts("beanstalkd 1.4.6 not found, skipping");
        exit(0);
    }

    progname=__func__;
    port = (rand()&0xfbff) + 1024;
    sprintf(portstr, "%d", port);
    muststart("beanstalkd-1.4.6", "-b", ctdir(), "-p", portstr);
    fd = mustdiallocal(port);
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

    kill(srvpid, 9);
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
benchputdeletesize(int n, int size)
{
    port = SERVER();
    fd = mustdiallocal(port);
    char buf[50], put[50];
    char body[size+1];
    memset(body, 'a', size);
    body[size] = 0;
    ctsetbytes(size);
    sprintf(put, "put 0 0 0 %d\r\n", size);
    int i;
    for (i = 0; i < n; i++) {
        mustsend(fd, put);
        mustsend(fd, body);
        mustsend(fd, "\r\n");
        ckrespsub(fd, "INSERTED ");
        sprintf(buf, "delete %d\r\n", i+1);
        mustsend(fd, buf);
        ckresp(fd, "DELETED\r\n");
    }
}


void
ctbenchputdelete8byte(int n)
{
    benchputdeletesize(n, 8);
}


void
ctbenchputdelete1k(int n)
{
    benchputdeletesize(n, 1024);
}


void
ctbenchputdelete8k(int n)
{
    benchputdeletesize(n, 8192);
}
