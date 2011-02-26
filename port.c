/* portability functions */

/* Copyright (C) 2009 Keith Rarick

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

#include "t.h"

#ifdef _NEED_POSIX_FALLOCATE
int
posix_fallocate(int fd, off_t offset, off_t len)
{
    off_t i;
    ssize_t w;
    off_t p;
    #define ZERO_BUF_SIZE 512
    char buf[ZERO_BUF_SIZE] = {}; /* initialize to zero */

    /* we only support a 0 offset */
    if (offset != 0) return EINVAL;

    if (len <= 0) return EINVAL;

    if (len % 512 != 0) {
        len += 512 - len % 512;
    }

    for (i = 0; i < len; i += w) {
        w = write(fd, &buf, ZERO_BUF_SIZE);
        if (w == -1) return errno;
    }

    p = lseek(fd, 0, SEEK_SET);
    if (p == -1) return errno;

    return 0;
}
#endif
