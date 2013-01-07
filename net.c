#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "dat.h"
#include "sd-daemon.h"

int
make_server_socket(char *host, char *port)
{
    int fd = -1, flags, r;
    struct linger linger = {0, 0};
    struct addrinfo *airoot, *ai, hints;

    /* See if we got a listen fd from systemd. If so, all socket options etc
     * are already set, so we check that the fd is a TCP listen socket and
     * return. */
    r = sd_listen_fds(1);
    if (r < 0) {
        return twarn("sd_listen_fds"), -1;
    }
    if (r > 0) {
        if (r > 1) {
            twarnx("inherited more than one listen socket;"
                   " ignoring all but the first");
        }
        fd = SD_LISTEN_FDS_START;
        r = sd_is_socket_inet(fd, 0, SOCK_STREAM, 1, 0);
        if (r < 0) {
            errno = -r;
            twarn("sd_is_socket_inet");
            return -1;
        }
        if (!r) {
            twarnx("inherited fd is not a TCP listen socket");
            return -1;
        }
        return fd;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    r = getaddrinfo(host, port, &hints, &airoot);
    if (r == -1)
      return twarn("getaddrinfo()"), -1;

    for(ai = airoot; ai; ai = ai->ai_next) {
      fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd == -1) {
        twarn("socket()");
        continue;
      }

      flags = fcntl(fd, F_GETFL, 0);
      if (flags < 0) {
        twarn("getting flags");
        close(fd);
        continue;
      }

      r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      if (r == -1) {
        twarn("setting O_NONBLOCK");
        close(fd);
        continue;
      }

      flags = 1;
      r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof flags);
      if (r == -1) {
        twarn("setting SO_REUSEADDR on fd %d", fd);
        close(fd);
        continue;
      }
      r = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof flags);
      if (r == -1) {
        twarn("setting SO_KEEPALIVE on fd %d", fd);
        close(fd);
        continue;
      }
      r = setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof linger);
      if (r == -1) {
        twarn("setting SO_LINGER on fd %d", fd);
        close(fd);
        continue;
      }
      r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof flags);
      if (r == -1) {
        twarn("setting TCP_NODELAY on fd %d", fd);
        close(fd);
        continue;
      }

      if (verbose) {
          char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV], *h = host, *p = port;
          r = getnameinfo(ai->ai_addr, ai->ai_addrlen,
                  hbuf, sizeof hbuf,
                  pbuf, sizeof pbuf,
                  NI_NUMERICHOST|NI_NUMERICSERV);
          if (!r) {
              h = hbuf;
              p = pbuf;
          }
          if (ai->ai_family == AF_INET6) {
              printf("bind %d [%s]:%s\n", fd, h, p);
          } else {
              printf("bind %d %s:%s\n", fd, h, p);
          }
      }
      r = bind(fd, ai->ai_addr, ai->ai_addrlen);
      if (r == -1) {
        twarn("bind()");
        close(fd);
        continue;
      }

      r = listen(fd, 1024);
      if (r == -1) {
        twarn("listen()");
        close(fd);
        continue;
      }

      break;
    }

    freeaddrinfo(airoot);

    if(ai == NULL)
      fd = -1;

    return fd;
}
