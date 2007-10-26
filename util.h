#ifndef util_h
#define util_h

#define min(a,b) ((a)<(b)?(a):(b))

int warn(const char *s);

void v();

#ifdef DEBUG
#define dprintf(fmt, args...) fprintf(stderr, fmt, ##args)
#else
#define dprintf(fmt, ...) (0)
#endif

#endif /*util_h*/
