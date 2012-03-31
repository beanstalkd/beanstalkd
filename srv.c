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

    sockinit((Handle)srvtick, s, 10*1000000); // 10ms

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

    sockmain();
    twarnx("sockmain");
    exit(1);
}


void
srvschedconn(Server *s, conn c)
{
    if (c->tickpos > -1) {
        heapremove(&s->conns, c->tickpos);
    }
    if (c->tickat) {
        heapinsert(&s->conns, c);
    }
}


void
srvaccept(Server *s, int ev)
{
    h_accept(s->sock.fd, ev, s);
}


void
srvtick(Server *s, int ev)
{
    prottick(s);
}
