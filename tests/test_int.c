/* integration tests */

#include "../t.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <event.h>
#include <errno.h>
#include "../cut.h"
#include "../dat.h"

static void testsrv(char*, char*);
static void forksrv(int*, int*);
static int copy(int, int);
static int diff(char*, int);
static int diallocal(int);

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
    {"sh-tests/pause-tube.commands", "sh-tests/pause-tube.expected"},
    {"sh-tests/small_delay.commands", "sh-tests/small_delay.expected"},
    {"sh-tests/too-big.commands", "sh-tests/too-big.expected"},
    {"sh-tests/ttr-large.commands", "sh-tests/ttr-large.expected"},
    {"sh-tests/zero_delay.commands", "sh-tests/zero_delay.expected"},
    {},
};


void
__CUT_BRINGUP__srv()
{
}


void
__CUT__srv_test()
{
    int i;

    for (i = 0; ts[i].cmd; i++) {
        testsrv(ts[i].cmd, ts[i].exp);
    }
}


void
__CUT_TAKEDOWN__srv()
{
}


static void
testsrv(char *cmd, char *exp)
{
    int status, port = 0, cfd, tfd, diffpid, srvpid = 0;

    job_data_size_limit = 10;

    progname = cmd;
    puts(cmd);
    forksrv(&port, &srvpid);
    if (port == -1 || srvpid == -1) {
        puts("forksrv failed");
        exit(1);
    }

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

    if (copy(cfd, tfd) == -1) {
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

    waitpid(diffpid, &status, 0);

    // wait until after diff has finished to kill srvpid
    kill(srvpid, 9);

    printf("diff status %d\n", status);
    ASSERT(status == 0, "diff status");
}


static void
forksrv(int *port, int *pid)
{
    int r, srvfd, len;
    struct sockaddr_in addr;
    struct event_base *ev_base;

    srvfd = make_server_socket("127.0.0.1", "0");
    if (srvfd == -1) return;

    len = sizeof(addr);
    r = getsockname(srvfd, (struct sockaddr*)&addr, (socklen_t*)&len);
    if (r == -1 || len > sizeof(addr)) return;

    *port = addr.sin_port;

    *pid = fork();
    if (*pid != 0) return;

    /* now in child */

    prot_init();
    ev_base = event_init();

    srv(srvfd); /* does not return */
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
copy(int dst, int src)
{
    char buf[4096];
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
    struct sockaddr_in addr = {AF_INET, port};

    r = inet_aton("127.0.0.1", &addr.sin_addr);
    if (!r) {
        errno = EINVAL;
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return -1;
    }

    r = connect(fd, (struct sockaddr*)&addr, sizeof addr);
    if (r == -1) {
        return -1;
    }

    return fd;
}
