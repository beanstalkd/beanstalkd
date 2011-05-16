#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "dat.h"

enum
{
    Infinity = 1 << 30
};

static void handle(Socket*, int, int);

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
    default:
        // check only for hangup
        ev.filter = EVFILT_READ;
        ev.fflags = NOTE_LOWAT;
        ev.data = Infinity;
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
            handle(evs[i].udata, evs[i].filter, evs[i].flags);
        }

    }
}


static void
handle(Socket *s, int filt, int flags)
{
    if (flags & EV_EOF) {
        s->f(s->x, 'h');
    } else if (filt == EVFILT_READ) {
        s->f(s->x, 'r');
    } else if (filt == EVFILT_WRITE) {
        s->f(s->x, 'w');
    }
}
