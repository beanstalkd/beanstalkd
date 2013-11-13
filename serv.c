#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "dat.h"

struct Server srv = {
    Portdef,
    NULL,
    NULL,
    {
        Filesizedef,
    },
};


void
srvserve(Server *s)
{
    int r;
    Socket *sock;
    int64 period;

    if (sockinit() == -1) {
        twarnx("sockinit");
        exit(1);
    }

    s->sock.x = s;
    s->sock.f = (Handle)srvaccept;
    s->conns.less = (Less)connless;
    s->conns.rec = (Record)connrec;

    r = listen(s->sock.fd, 1024);
    if (r == -1) {
        twarn("listen");
        return;
    }

    r = sockwant(&s->sock, 'r');
    if (r == -1) {
        twarn("sockwant");
        exit(2);
    }


    period = prottick(s);

    int64 t = nanoseconds();
    for (;;) {
        int64 t1 = nanoseconds();
        if (t1-t > 10*1000000) {  // 10ms
            period = prottick(s);
            t = t1;
        }

        int rw = socknext(&sock, period);
        if (rw == -1) {
            twarnx("socknext");
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
