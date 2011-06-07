#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include "dat.h"

int64
nanoseconds(void)
{
    int r;
    struct timeval tv;

    r = gettimeofday(&tv, 0);
    if (r != 0) return warnx("gettimeofday"), -1; // can't happen

    return ((int64)tv.tv_sec)*1000000000 + ((int64)tv.tv_usec)*1000;
}
