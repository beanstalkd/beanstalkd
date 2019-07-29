#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

struct Server srv = {
    .port = Portdef,
    .wal = {
        .filesize = Filesizedef,
    },
};


void
srvserve(Server *s)
{
    int r;
    Socket *sock;
    int64 period;

    if (sockinit() == -1) {
        twarn("sockinit");
        exit(1);
    }

    s->sock.x = s;
    s->sock.f = (Handle)srvaccept;
    s->conns.less = conn_less;
    s->conns.setpos = conn_setpos;

    r = listen(s->sock.fd, 1024);
    if (r == -1) {
        twarnerr("listen");
        return;
    }

    r = sockwant(&s->sock, 'r');
    if (r == -1) {
        twarnerr("sockwant");
        exit(2);
    }


    for (;;) {
        period = prottick(s);

        int rw = socknext(&sock, period);
        if (rw == -1) {
            twarn("socknext");
            exit(1);
        }

        if (rw) {
            sock->f(sock->x, rw);
        }
    }
}


void
srvaccept(Server *s, int ev)
{
    h_accept(s->sock.fd, ev, s);
}
