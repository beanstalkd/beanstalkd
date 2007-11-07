/* net.c - stupid boilerplate shit that I shouldn't have to write */

#include <stdio.h>
#include <errno.h>

#include "net.h"
#include "util.h"

static int listen_socket = -1;
static struct event listen_evq;
static evh accept_handler;

int
make_server_socket(int host, int port)
{
    int fd, flags, r;
    struct linger linger = {0, 0};
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return twarn("socket()"), -1;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return twarn("getting flags"), close(fd), -1;

    r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (flags < 0) return twarn("setting O_NONBLOCK"), close(fd), -1;

    flags = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof flags);
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof flags);
    setsockopt(fd, SOL_SOCKET, SO_LINGER,   &linger, sizeof linger);
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof flags);

    /*memset(&addr, 0, sizeof addr);*/

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(host);
    r = bind(fd, (struct sockaddr *) &addr, sizeof addr);
    if (r == -1) return twarn("bind()"), close(fd), -1;

    r = listen(fd, 1024);
    if (r == -1) return twarn("listen()"), close(fd), -1;

    return listen_socket = fd;
}

void
brake()
{
    int r;

    twarnx("too many connections; putting on the brakes");

    r = event_del(&listen_evq);
    if (r == -1) twarn("event_del()");

    r = listen(listen_socket, 0);
    if (r == -1) twarn("listen()");
}

void
unbrake(evh h)
{
    int r;

    twarnx("releasing the brakes");

    accept_handler = h ? : accept_handler;
    event_set(&listen_evq, listen_socket, EV_READ | EV_PERSIST,
              accept_handler, &listen_evq);

    errno = 0;
    r = event_add(&listen_evq, NULL);
    if (r == -1) twarn("event_add()");

    r = listen(listen_socket, 1024);
    if (r == -1) twarn("listen()");
}

