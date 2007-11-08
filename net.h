#ifndef net_h
#define net_h

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

#include "event.h"
#include "conn.h"

int make_server_socket(int host, int port);

void brake();
void unbrake(evh h);
void set_main_timeout(time_t deadline);

#endif /*net_h*/
