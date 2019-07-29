#include "dat.h"
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>

static void
su(const char *user) 
{
    errno = 0;
    struct passwd *pwent = getpwnam(user);
    if (errno) {
        twarnerr("getpwnam(\"%s\")", user);
        exit(32);
    }
    if (!pwent) {
        twarn("getpwnam(\"%s\"): no such user", user);
        exit(33);
    }

    int r = setgid(pwent->pw_gid);
    if (r == -1) {
        twarnerr("setgid(%d \"%s\")", pwent->pw_gid, user);
        exit(34);
    }

    r = setuid(pwent->pw_uid);
    if (r == -1) {
        twarnerr("setuid(%d \"%s\")", pwent->pw_uid, user);
        exit(34);
    }
}

static void
set_sig_handlers()
{
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    int r = sigemptyset(&sa.sa_mask);
    if (r == -1) {
        twarnerr("sigemptyset()");
        exit(111);
    }

    r = sigaction(SIGPIPE, &sa, 0);
    if (r == -1) {
        twarnerr("sigaction(SIGPIPE)");
        exit(111);
    }

    sa.sa_handler = enter_drain_mode;
    r = sigaction(SIGUSR1, &sa, 0);
    if (r == -1) {
        twarnerr("sigaction(SIGUSR1)");
        exit(111);
    }
}

int
main(int argc, char **argv)
{
    UNUSED_PARAMETER(argc);

    progname = argv[0];
    setlinebuf(stdout);
    optparse(&srv, argv+1);

    if (verbose) {
        printf("pid %d\n", getpid());
    }

    int r = make_server_socket(srv.addr, srv.port);
    if (r == -1) {
        twarn("make_server_socket()");
        exit(111);
    }

    srv.sock.fd = r;

    prot_init();

    if (srv.user)
        su(srv.user);
    set_sig_handlers();

    if (srv.wal.use) {
        // We want to make sure that only one beanstalkd tries
        // to use the wal directory at a time. So acquire a lock
        // now and never release it.
        if (!waldirlock(&srv.wal)) {
            twarn("failed to lock wal dir %s", srv.wal.dir);
            exit(10);
        }

        Job list = {.prev=NULL, .next=NULL};
        list.prev = list.next = &list;
        walinit(&srv.wal, &list);
        r = prot_replay(&srv, &list);
        if (!r) {
            twarn("failed to replay log");
            exit(1);
        }
    }

    srvserve(&srv);
    exit(0);
}
