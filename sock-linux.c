#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>
#include "dat.h"

#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

static int epfd;


int
sockinit(void)
{
    epfd = epoll_create(1);
    if (epfd == -1) {
        twarn("epoll_create");
        return -1;
    }
    return 0;
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


int
socknext(Socket **s, int64 timeout)
{
    int r;
    struct epoll_event ev;

    r = epoll_wait(epfd, &ev, 1, (int)(timeout/1000000));
    if (r == -1 && errno != EINTR) {
        twarn("epoll_wait");
        exit(1);
    }

    if (r) {
        *s = ev.data.ptr;
        if (ev.events & (EPOLLIN|EPOLLHUP|EPOLLRDHUP)) {
            return 'r';
        } else if (ev.events & EPOLLOUT) {
            return 'w';
        }
    }
    return 0;
}
