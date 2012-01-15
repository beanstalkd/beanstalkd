#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include "ct/ct.h"
#include "dat.h"

static void testsrv(char*, char*, int);
static void forksrv(int*, int*);
static int copy(int, int, int);
static int diff(char*, int);
static int diallocal(int);
static void cleanup(int sig);
static void mustsend(int fd, char* cmd);
static void ckresp(int fd, char* exp);
static void writefull(int fd, char *s, int n);
static void readfull(int fd, char *b, int n);

typedef struct T T;
struct T {
    char *cmd;
    char *exp;
};

static T ts[] = {
    {"sh-tests/allow-underscore.commands", "sh-tests/allow-underscore.expected"},
    {"sh-tests/delete_ready.commands", "sh-tests/delete_ready.expected"},
    {"sh-tests/multi-tube.commands", "sh-tests/multi-tube.expected"},
    {"sh-tests/no_negative_delays.commands", "sh-tests/no_negative_delays.expected"},
    {"sh-tests/omit-time-left.commands", "sh-tests/omit-time-left.expected"},
    //{"sh-tests/pause-tube.commands", "sh-tests/pause-tube.expected"},
    {"sh-tests/small_delay.commands", "sh-tests/small_delay.expected"},
    {"sh-tests/stats_tube.commands", "sh-tests/stats_tube.expected"},
    {"sh-tests/too-big.commands", "sh-tests/too-big.expected"},
    {"sh-tests/ttr-large.commands", "sh-tests/ttr-large.expected"},
    {"sh-tests/ttr-small.commands", "sh-tests/ttr-small.expected"},
    {"sh-tests/zero_delay.commands", "sh-tests/zero_delay.expected"},
    {},
};

static int srvpid;
static int timeout = 100*1000000; // 100ms


void
cttestsrv()
{
    int i;

    for (i = 0; ts[i].cmd; i++) {
        testsrv(ts[i].cmd, ts[i].exp, 4096);
        testsrv(ts[i].cmd, ts[i].exp, 1);
    }
}


static void
testsrv(char *cmd, char *exp, int bufsiz)
{
    int diffst, srvst, port = 0, cfd, tfd, diffpid;
    struct sigaction sa = {};

    job_data_size_limit = 10;

    progname = cmd;
    puts(cmd);
    forksrv(&port, &srvpid);
    if (port == -1 || srvpid == -1) {
        puts("forksrv failed");
        exit(1);
    }

    // Fail if this test takes more than 10 seconds.
    // If we have trouble installing the timeout,
    // just proceed anyway.
    sa.sa_handler = cleanup;
    sigaction(SIGALRM, &sa, 0);
    alarm(10);

    cfd = diallocal(port);
    if (cfd == -1) {
        twarn("diallocal");
        kill(srvpid, 9);
        exit(1);
    }

    tfd = open(cmd, O_RDONLY, 0);
    if (tfd == -1) {
        twarn("open");
        kill(srvpid, 9);
        exit(1);
    }

    if (copy(cfd, tfd, bufsiz) == -1) {
        twarn("copy");
        kill(srvpid, 9);
        exit(1);
    }

    diffpid = diff(exp, cfd);
    if (diffpid == -1) {
        twarn("diff");
        kill(srvpid, 9);
        exit(1);
    }

    waitpid(diffpid, &diffst, 0);

    // wait until after diff has finished to kill srvpid
    kill(srvpid, 9);
    waitpid(srvpid, &srvst, 0);
    assertf(WIFSIGNALED(srvst) && WTERMSIG(srvst) == 9,
            "status %d, signal %d",
            WEXITSTATUS(srvst),
            WTERMSIG(srvst));

    assertf(diffst == 0, "was %d", diffst);
}


static void
killsrv(void)
{
    if (srvpid > 1) {
        kill(srvpid, 9);
    }
}


void
cttestreservewithtimeout2conn()
{
    int port = 0, cfd0, cfd1;
    struct sigaction sa = {};

    job_data_size_limit = 10;

    progname = __func__;
    forksrv(&port, &srvpid);
    if (port == -1 || srvpid == -1) {
        puts("forksrv failed");
        exit(1);
    }

    atexit(killsrv);

    // Fail if this test takes more than 10 seconds.
    // If we have trouble installing the timeout,
    // just proceed anyway.
    sa.sa_handler = cleanup;
    sigaction(SIGALRM, &sa, 0);
    alarm(10);

    cfd0 = diallocal(port);
    if (cfd0 == -1) {
        twarn("diallocal");
        exit(1);
    }

    cfd1 = diallocal(port);
    if (cfd1 == -1) {
        twarn("diallocal");
        exit(1);
    }

    mustsend(cfd0, "watch foo\r\n");
    ckresp(cfd0, "WATCHING 2\r\n");
    mustsend(cfd0, "reserve-with-timeout 1\r\n");
    mustsend(cfd1, "watch foo\r\n");
    ckresp(cfd1, "WATCHING 2\r\n");
    timeout = 1100000000; // 1.1s
    ckresp(cfd0, "TIMED_OUT\r\n");
}


static void
forksrv(int *port, int *pid)
{
    int r, len;
    Srv s = {};
    struct sockaddr_in addr;

    s.sock.fd = make_server_socket("127.0.0.1", "0");
    if (s.sock.fd == -1) return;

    len = sizeof(addr);
    r = getsockname(s.sock.fd, (struct sockaddr*)&addr, (socklen_t*)&len);
    if (r == -1 || len > sizeof(addr)) return;

    *port = addr.sin_port;

    *pid = fork();
    if (*pid != 0) return;

    /* now in child */

    prot_init();

    srv(&s); /* does not return */
    exit(1); /* satisfy the compiler */
}


static int
diff(char *f0, int fd)
{
    int pid;

    pid = fork();
    if (pid != 0) return pid;

    /* now in child */

    dup2(fd, 0);
    close(fd);

    execlp("diff", "diff", f0, "-", (char*)0);
    /* not reached */
    exit(1);
}


static int
copy(int dst, int src, int bs)
{
    char buf[bs];
    int r, w, c;

    for (;;) {
        r = read(src, buf, sizeof buf);
        if (r == -1) return -1;
        if (r == 0) break;

        for (w = 0; w < r; w += c) {
            c = write(dst, buf+w, r-w);
            if (c == -1) return -1;
        }
    }
    return 0;
}


static int
diallocal(int port)
{
    int r, fd;
    struct sockaddr_in addr = {};

    addr.sin_family = AF_INET;
    addr.sin_port = port;
    r = inet_aton("127.0.0.1", &addr.sin_addr);
    if (!r) {
        errno = EINVAL;
        return -1;
    }

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return -1;
    }

    r = connect(fd, (struct sockaddr*)&addr, sizeof addr);
    if (r == -1) {
        return -1;
    }

    return fd;
}


static void
cleanup(int sig)
{
    puts("timed out");
    if (srvpid > 0) {
        kill(srvpid, 9);
    }
    exit(1);
}


static void
mustsend(int fd, char *s)
{
    writefull(fd, s, strlen(s));
    printf(">%d %s", fd, s);
    fflush(stdout);
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
readfull(int fd, char *b, int n)
{
    int r;
    fd_set rfd;
    struct timeval tv;

    for (; n; n -= r) {
        FD_ZERO(&rfd);
        FD_SET(fd, &rfd);
        tv.tv_sec = timeout / 1000000000;
        tv.tv_usec = (timeout/1000) % 1000000;
        r = select(fd+1, &rfd, (void*)0, (void*)0, &tv);
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

        r = read(fd, b, n);
        if (r == -1) {
            perror("write");
            exit(1);
        }
        b += r;
    }
}


static void
ckresp(int fd, char *exp)
{
    printf("<%d ", fd);
    fflush(stdout);
    char c;
    while (*exp) {
        readfull(fd, &c, 1);
        assert(c == *exp);
        putc(c, stdout);
        fflush(stdout);
        exp++;
    }
}
