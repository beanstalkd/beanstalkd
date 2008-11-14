/* beanstalk - fast, general-purpose work queue */

/* Copyright (C) 2007 Keith Rarick and Philotic Inc.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <event.h>

#include "net.h"
#include "util.h"
#include "prot.h"
#include "version.h"
#include "binlog.h"

static char *user = NULL;
static int detach = 0;
static int port = 11300;
static struct in_addr host_addr = { INADDR_ANY };

static void
nullfd(int fd, int flags)
{
    int r;

    close(fd);
    r = open("/dev/null", flags);
    if (r != fd) twarn("open(\"/dev/null\")"), exit(1);
}

static void
dfork()
{
    pid_t p;

    p = fork();
    if (p == -1) exit(1);
    if (p) exit(0);
}

static void
daemonize()
{
    chdir("/");
    nullfd(0, O_RDONLY);
    nullfd(1, O_WRONLY);
    nullfd(2, O_WRONLY);
    umask(0);
    dfork();
    setsid();
    dfork();
}

static void
su(const char *user) {
    int r;
    struct passwd *pwent;

    errno = 0;
    pwent = getpwnam(user);
    if (errno) twarn("getpwnam(\"%s\")", user), exit(32);
    if (!pwent) twarnx("getpwnam(\"%s\"): no such user", user), exit(33);

    r = setgid(pwent->pw_gid);
    if (r == -1) twarn("setgid(%d \"%s\")", pwent->pw_gid, user), exit(34);

    r = setuid(pwent->pw_uid);
    if (r == -1) twarn("setuid(%d \"%s\")", pwent->pw_uid, user), exit(34);
}

void
exit_cleanly(int sig)
{
    binlog_close();
    exit(0);
}


static void
set_sig_handlers()
{
    int r;
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    r = sigemptyset(&sa.sa_mask);
    if (r == -1) twarn("sigemptyset()"), exit(111);

    r = sigaction(SIGPIPE, &sa, 0);
    if (r == -1) twarn("sigaction(SIGPIPE)"), exit(111);

    sa.sa_handler = enter_drain_mode;
    r = sigaction(SIGUSR1, &sa, 0);
    if (r == -1) twarn("sigaction(SIGUSR1)"), exit(111);

    sa.sa_handler = exit_cleanly;
    r = sigaction(SIGINT, &sa, 0);
    if (r == -1) twarn("sigaction(SIGINT)"), exit(111);
}

/* This is a workaround for a mystifying workaround in libevent's epoll
 * implementation. The epoll_init() function creates an epoll fd with space to
 * handle RLIMIT_NOFILE - 1 fds, accompanied by the following puzzling comment:
 * "Solaris is somewhat retarded - it's important to drop backwards
 * compatibility when making changes. So, don't dare to put rl.rlim_cur here."
 * This is presumably to work around a bug in Solaris, but it has the
 * unfortunate side-effect of causing epoll_ctl() (and, therefore, event_add())
 * to fail for a valid fd if we have hit the limit of open fds. That makes it
 * hard to provide reasonable behavior in that situation. So, let's reduce the
 * real value of RLIMIT_NOFILE by one, after epoll_init() has run. */
static void
nudge_fd_limit()
{
    int r;
    struct rlimit rl;

    r = getrlimit(RLIMIT_NOFILE, &rl);
    if (r != 0) twarn("getrlimit(RLIMIT_NOFILE)"), exit(2);

    rl.rlim_cur--;

    r = setrlimit(RLIMIT_NOFILE, &rl);
    if (r != 0) twarn("setrlimit(RLIMIT_NOFILE)"), exit(2);
}

static void
usage(char *msg, char *arg)
{
    if (arg) warnx("%s: %s", msg, arg);
    fprintf(stderr, "Use: %s [OPTIONS]\n"
            "\n"
            "Options:\n"
            " -d       detach\n"
            " -b DIR   binlog directory\n"
            " -l ADDR  listen on address (default is 0.0.0.0)\n"
            " -p PORT  listen on port (default is 11300)\n"
            " -u USER  become user and group\n"
            " -z SIZE  set the maximum job size in bytes (default is %d)\n"
            " -v       show version information\n"
            " -h       show this help\n",
            progname, JOB_DATA_SIZE_LIMIT_DEFAULT);
    exit(arg ? 5 : 0);
}

static size_t
parse_size_t(char *str)
{
    char r, x;
    size_t size;

    r = sscanf(str, "%zu%c", &size, &x);
    if (1 != r) usage("invalid size", str);
    return size;
}

static int
parse_port(char *portstr)
{
    int port;
    char *end;

    errno = 0;
    port = strtol(portstr, &end, 10);
    if (end == portstr) usage("invalid port", portstr);
    if (end[0] != 0) usage("invalid port", portstr);
    if (errno) usage("invalid port", portstr);

   return port;
}

static struct in_addr
parse_host(char *hoststr)
{
    int r;
    struct in_addr addr;

    r = inet_aton(hoststr, &addr);
    if (!r) usage("invalid address", hoststr);

    return addr;
}

static void
opts(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') usage("unknown option", argv[i]);
        if (argv[i][1] == 0 || argv[i][2] != 0) usage("unknown option",argv[i]);
        switch (argv[i][1]) {
            case 'd':
                detach = 1;
                break;
            case 'p':
                port = parse_port(argv[++i]);
                break;
            case 'l':
                host_addr = parse_host(argv[++i]);
                break;
            case 'z':
                job_data_size_limit = parse_size_t(argv[++i]);
                break;
            case 'u':
                user = argv[++i];
                break;
            case 'b':
                binlog_dir = argv[++i];
                break;
            case 'h':
                usage(NULL, NULL);
            case 'v':
                printf("beanstalkd %s\n", BEAN_VERSION);
                exit(0);
            default:
                usage("unknown option", argv[i]);
        }
    }
}

int
main(int argc, char **argv)
{
    int r;

    progname = argv[0];
    opts(argc, argv);

    job_init();
    prot_init();

    r = make_server_socket(host_addr, port);
    if (r == -1) twarnx("make_server_socket()"), exit(111);

    if (user) su(user);
    if (detach) daemonize();
    event_init();
    set_sig_handlers();
    nudge_fd_limit();

    unbrake((evh) h_accept);
    prot_replay_binlog();
    binlog_init();

    event_dispatch();
    binlog_close();
    twarnx("got here for some reason");
    return 0;
}
