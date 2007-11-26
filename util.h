#ifndef util_h
#define util_h

#include <err.h>

#define min(a,b) ((a)<(b)?(a):(b))

void v();

#define twarn(fmt, args...) warn("%s:%d in %s: " fmt, \
                                 __FILE__, __LINE__, __func__, ##args)
#define twarnx(fmt, args...) warnx("%s:%d in %s: " fmt, \
                                   __FILE__, __LINE__, __func__, ##args)

#ifdef DEBUG
#define dprintf(fmt, args...) ((void) fprintf(stderr, fmt, ##args))
#else
#define dprintf(fmt, ...) ((void) 0)
#endif

#endif /*util_h*/
