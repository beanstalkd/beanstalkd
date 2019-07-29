#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <port.h>
#include "dat.h"

static int portfd;
static char buf0[512]; /* buffer of zeros */

/* Allocate disk space.
 * Expects fd's offset to be 0; may also reset fd's offset to 0.
 * Returns 0 on success, and a positive errno otherwise. */
int
rawfalloc(int fd, int len)
{
    int i, w;

    for (i = 0; i < len; i += w) {
        w = write(fd, buf0, sizeof buf0);
        if (w == -1) return errno;
    }

    lseek(fd, 0, 0); /* do not care if this fails */

    return 0;
}


int
sockinit(void)
{
    portfd = port_create();
    if (portfd == -1) {
        twarnerr("port_create");
        return -1;
    }
    return 0;
}


int
sockwant(Socket *s, int rw)
{
    int events = 0;

    if (rw) {
        switch (rw) {
        case 'r':
            events |= POLLIN;
            break;
        case 'w':
            events |= POLLOUT;
            break;
        }
    }

    events |= POLLPRI;

    if (!s->added && !rw) {
        return 0;
    } else if (!s->added && rw) {
        s->added = 1;
        return port_associate(portfd, PORT_SOURCE_FD, s->fd, events, (void *)s);
    } else if (!rw) {
        return port_dissociate(portfd, PORT_SOURCE_FD, s->fd);
    } else {
        port_dissociate(portfd, PORT_SOURCE_FD, s->fd);
        return port_associate(portfd, PORT_SOURCE_FD, s->fd, events, (void *)s);
    }
}


int
socknext(Socket **s, int64 timeout)
{
    int r;
    uint_t n = 1;
    struct port_event pe;
    struct timespec ts;

    ts.tv_sec = timeout / 1000000000;
    ts.tv_nsec = timeout % 1000000000;
    r = port_getn(portfd, &pe, 1, &n, &ts);
    if (r == -1 && errno != ETIME && errno != EINTR) {
        twarnerr("port_getn");
        return -1;
    }

    if (r == 0) {
        *s = pe.portev_user;
        if (pe.portev_events & POLLHUP) {
            return 'h';
        } else if (pe.portev_events & POLLIN) {
            if (sockwant(*s, 'r') == -1) {
                return -1;
            }
            return 'r';
        } else if (pe.portev_events & POLLOUT) {
            if (sockwant(*s, 'w') == -1) {
                return -1;
            }
            return 'w';
        }
    }

    return 0;
}
