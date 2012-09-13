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

static int kq;


int
sockinit(void)
{
    kq = kqueue();
    if (kq == -1) {
        twarn("kqueue");
        return -1;
    }
    return 0;
}


int
sockwant(Socket *s, int rw)
{
    int n = 0;
    struct kevent evs[2] = {}, *ev = evs;
    struct timespec ts = {};

    if (s->added) {
        ev->ident = s->fd;
        ev->filter = s->added;
        ev->flags = EV_DELETE;
        ev++;
        n++;
    }

    if (rw) {
        ev->ident = s->fd;
        switch (rw) {
        case 'r':
            ev->filter = EVFILT_READ;
            break;
        case 'w':
            ev->filter = EVFILT_WRITE;
            break;
        default:
            // check only for hangup
            ev->filter = EVFILT_READ;
            ev->fflags = NOTE_LOWAT;
            ev->data = Infinity;
        }
        ev->flags = EV_ADD;
        ev->udata = s;
        s->added = ev->filter;
        ev++;
        n++;
    }

    return kevent(kq, evs, n, NULL, 0, &ts);
}


int
socknext(Socket **s, int64 timeout)
{
    int r;
    struct kevent ev;
    static struct timespec ts;

    ts.tv_sec = timeout / 1000000000;
    ts.tv_nsec = timeout % 1000000000;
    r = kevent(kq, NULL, 0, &ev, 1, &ts);
    if (r == -1 && errno != EINTR) {
        twarn("kevent");
        return -1;
    }

    if (r > 0) {
        *s = ev.udata;
        switch (ev.filter) {
        case EVFILT_READ:
            return 'r';
        case EVFILT_WRITE:
            return 'w';
        }
    }
    return 0;
}
