// Copyright 2011 Keith Rarick. See file COPYING for details.

#include <stdlib.h>
#include <sys/resource.h>
#include <event.h>
#include "dat.h"

static void handleev(const int fd, const short ev, Socket *s);
static void nudge_fd_limit();
static Handle tick;
static void   *tickval;

static struct timeval ival;


void
sockinit(Handle f, void *x, int64 i)
{
    tick = f;
    tickval = x;
    init_timeval(&ival, i);
    event_init();
    nudge_fd_limit();
}


int
sockwant(Socket *s, int rw)
{
    int r;

    if (s->added) {
        r = event_del(&s->evq);
        if (r == -1) {
            return -1;
        }
    }
    s->added = 1;
    if (rw) {
        int mask = 0;

        switch (rw) {
        case 't':
            // fake a global timeout with libevent
            event_set(&s->evq, s->fd, EV_READ|EV_PERSIST, (evh)handleev, s);
            return event_add(&s->evq, &ival);
        case 'r':
            mask = EV_READ;
            break;
        case 'w':
            mask = EV_WRITE;
            break;
        }
        event_set(&s->evq, s->fd, mask|EV_PERSIST, (evh)handleev, s);
        return event_add(&s->evq, 0);
    }
    return 0;
}


void
sockmain()
{
    event_dispatch();
}


static void
handleev(const int fd, const short ev, Socket *s)
{
    switch (ev) {
    case EV_TIMEOUT:
        // libevent removes the fd if it timed out, so put it back
        event_add(&s->evq, &ival);
        tick(tickval, 0);
        break;
    case EV_READ:
        s->f(s->x, 'r');
        break;
    case EV_WRITE:
        s->f(s->x, 'w');
        break;
    }
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
