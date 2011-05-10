#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "dat.h"

static void handle(Socket *s, int events);

static Handle          tick;
static void            *tickval;
static int             kq;
static int64           ival;
static struct timespec ivalts;


void
sockinit(Handle f, void *x, int64 ns)
{
    tick = f;
    tickval = x;
    ival = ns;
    ivalts.tv_sec = ns / 1000000000;
    ivalts.tv_nsec = ns % 1000000000;
    kq = kqueue();
    if (kq == -1) {
        twarn("kqueue");
        exit(1);
    }
}


int
sockwant(Socket *s, int rw)
{
    struct kevent ev = {};
    struct timespec ts = {};

    if (!s->added && !rw) {
        return 0;
    } else if (rw) {
        s->added = 1;
        ev.flags = EV_ADD;
    } else {
        ev.flags = EV_DELETE;
    }

    switch (rw) {
    case 'r':
        ev.filter = EVFILT_READ;
        break;
    case 'w':
        ev.filter = EVFILT_WRITE;
        break;
    }
    ev.ident = s->fd;
    ev.udata = s;
    return kevent(kq, &ev, 1, NULL, 0, &ts);
}


void
sockmain()
{
    int i, r, n = 500;
    int64 e, t = nanoseconds();
    struct kevent evs[n];

    for (;;) {
        r = kevent(kq, NULL, 0, evs, n, &ivalts);
        if (r == -1 && errno != EINTR) {
            twarn("kevent");
            exit(1);
        }

        // should tick?
        e = nanoseconds();
        if (e-t > ival) {
            tick(tickval, 0);
            t = e;
        }

        for (i=0; i<r; i++) {
            handle(evs[i].udata, evs[i].filter);
        }

    }
}


static void
handle(Socket *s, int filt)
{
    switch (filt) {
    case EVFILT_READ:
        s->f(s->x, 'r');
        break;
    case EVFILT_WRITE:
        s->f(s->x, 'w');
        break;
    }
}
