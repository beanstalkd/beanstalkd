/* event.h - wrapper for libevent's event.h */

#ifndef event_h
#define event_h

#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

typedef void(*evh)(int, short, void *);

#endif /*event_h*/
