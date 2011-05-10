#define _XOPEN_SOURCE 600
#include <stdint.h>
#include <fcntl.h>
#include "dat.h"

/* Allocate disk space.
 * Expects fd's offset to be 0; may also reset fd's offset to 0.
 * Returns 0 on success, and a positive errno otherwise. */
int
falloc(int fd, int len)
{
    return posix_fallocate(fd, 0, len);
}
