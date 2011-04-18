// Copyright 2011 Keith Rarick. See file COPYING for details.

#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>
#include <event.h>
#include "dat.h"

static void handle(Socket *s, int events);

static Handle tick;
static void   *tickval;
static int    epfd;
static int    ival; // ms


void
sockinit(Handle f, void *x, int64 ns)
{
    tick = f;
    tickval = x;
    ival = ns / 1000000;
    epfd = epoll_create1(0);
    if (epfd == -1) {
        twarn("epoll_create");
        exit(1);
    }
}


int
sockwant(Socket *s, int rw)
{
    int op;
    struct epoll_event ev = {};

    if (!s->added && !rw) {
        return 0;
    } else if (!s->added && rw) {
        s->added = 1;
        op = EPOLL_CTL_ADD;
    } else if (!rw) {
        op = EPOLL_CTL_DEL;
    } else {
        op = EPOLL_CTL_MOD;
    }

    switch (rw) {
    case 'r':
        ev.events = EPOLLIN;
        break;
    case 'w':
        ev.events = EPOLLOUT;
        break;
    }
    ev.events |= EPOLLRDHUP | EPOLLPRI;
    ev.data.ptr = s;

    return epoll_ctl(epfd, op, s->fd, &ev);
}


void
sockmain()
{
    int i, r, n = 500;
    int64 e, t = nanoseconds();
    struct epoll_event evs[n];

    for (;;) {
        r = epoll_wait(epfd, evs, n, ival);
        if (r == -1 && errno != EINTR) {
            twarn("epoll_wait");
            exit(1);
        }

        // should tick?
        e = nanoseconds();
        if ((e-t) / 1000000 > ival) {
            tick(tickval, 0);
            t = e;
        }

        for (i=0; i<r; i++) {
            handle(evs[i].data.ptr, evs[i].events);
        }

    }
}


static void
handle(Socket *s, int evset)
{
    int c = 0;

    if (evset & EPOLLIN) {
        c = 'r';
    } else if (evset & EPOLLOUT) {
        c = 'w';
    }

    s->f(s->x, c);
}
