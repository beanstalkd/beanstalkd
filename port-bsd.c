#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "dat.h"

static char buf0[512]; /* buffer of zeros */

/* Allocate disk space.
 * Expects fd's offset to be 0; may also reset fd's offset to 0.
 * Returns 0 on success, and a positive errno otherwise. */
int
falloc(int fd, int len)
{
    int i, w;

    for (i = 0; i < len; i += w) {
        w = write(fd, buf0, sizeof buf0);
        if (w == -1) return errno;
    }

    lseek(fd, 0, 0); /* do not care if this fails */

    return 0;
}
