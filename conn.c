/* conn.c - network connection state */

#include <stdlib.h>
#include <stdio.h>

#include "conn.h"
#include "net.h"
#include "util.h"
#include "beanstalkd.h"

conn
make_conn(int fd, char start_state)
{
    conn c;

    c = malloc(sizeof(struct conn));
    if (!c) return warn("OOM"), NULL;

    c->fd = fd;
    c->state = start_state;
    c->cmd_read = 0;
    c->in_job = NULL;
    c->reserved_job = NULL;

    return c;
}

int
conn_update_evq(conn c, const int flags, evh handler)
{
    int r;

    if (!c) return -1;

    if (c->evq.ev_flags == flags) return 0;

    /* If it's been added, try to delete it first */
    if (c->evq.ev_base) {
        r = event_del(&c->evq);
        if (r == -1) return -1;
    }

    /* set the flags and handler, but don't change any existing handler */
    event_set(&c->evq, c->fd, flags, c->evq.ev_callback ? : handler, c);

    r = event_add(&c->evq, NULL);
    if (r == -1) return -1;

    return 0;
}

void
conn_close(conn c)
{
    fprintf(stderr, "closing conn %d\n", c->fd);
    event_del(&c->evq);

    close(c->fd);
    if (c->reserved_job) enqueue_job(c->reserved_job);

    free(c);
}
