/* Copyright 2011 Keith Rarick

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "dat.h"


void
srv(Srv *s)
{
    int r;

    sockinit((Handle)srvtick, s, 10*1000000); // 10ms

    s->sock.x = s;
    s->sock.f = (Handle)srvaccept;
    s->conns.cmp = (Compare)conncmp;
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
srvschedconn(Srv *s, conn c)
{
    if (c->tickpos > -1) {
        heapremove(&s->conns, c->tickpos);
    }
    if (c->tickat) {
        heapinsert(&s->conns, c);
    }
}


void
srvaccept(Srv *s, int ev)
{
    h_accept(s->sock.fd, ev, s);
}


void
srvtick(Srv *s, int ev)
{
    prottick(s);
}
