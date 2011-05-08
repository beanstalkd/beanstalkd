/* Copyright 2011 Keith Rarick

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
