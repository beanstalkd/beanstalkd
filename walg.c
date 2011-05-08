/* Copyright (C) 2008 Graham Barr

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
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include "dat.h"


// Reads w->dir for files matching binlog.NNN,
// sets w->next to the next unused number, and
// returns the minimum number.
// If no files are found, sets w->next to 1 and
// returns a large number.
static int
walscandir(Wal *w)
{
    static char base[] = "binlog.";
    static const int len = sizeof(base) - 1;
    DIR *d;
    struct dirent *e;
    int min = 1<<30;
    int max = 0;
    int n;
    char *p;

    d = opendir(w->dir);
    if (!d) return min;

    while ((e = readdir(d))) {
        if (strncmp(e->d_name, base, len) == 0) {
            n = strtol(e->d_name+len, &p, 10);
            if (p && *p == '\0') {
                if (n > max) max = n;
                if (n < min) min = n;
            }
        }
    }

    closedir(d);
    w->next = max + 1;
    return min;
}


void
walgc(Wal *w)
{
    File *f;

    while (w->head && !w->head->refs) {
        f = w->head;
        w->head = f->next;
        if (w->tail == f) {
            w->tail = f->next; // also, f->next == NULL
        }

        unlink(f->path);
        free(f->path);
        free(f);
    }
}


// returns 1 on success, 0 on error.
static int
usenext(Wal *w)
{
    File *f;

    f = w->cur;
    if (!f->next) {
        twarnx("there is no next wal file");
        return 0;
    }

    w->cur = f->next;
    filewclose(f);
    return 1;
}


static void
walsync(Wal *w)
{
    int64 now;

    now = nanoseconds();
    if (w->wantsync && now >= w->lastsync+w->syncrate) {
        w->lastsync = now;
        if (fsync(w->cur->fd) == -1) {
            twarn("fsync");
        }
    }
}


// Walwrite writes j to the log w (if w is enabled).
// On failure, walwrite disables w and returns 0; on success, it returns 1.
// Unlke walresv*, walwrite should never fail because of a full disk.
// If w is disabled, then walwrite takes no action and returns 1.
int
walwrite(Wal *w, job j)
{
    int r = 0;

    if (!w->use) return 1;
    if (w->cur->resv > 0 || usenext(w)) {
        if (j->file) {
            r = filewrjobshort(w->cur, j);
        } else {
            r = filewrjobfull(w->cur, j);
        }
    }
    if (!r) {
        filewclose(w->cur);
        w->use = 0;
        return 0;
    }

    walsync(w);
    return r;
}


static int
makenextfile(Wal *w)
{
    File *f;

    f = new(File);
    if (!f) {
        twarnx("OOM");
        return 0;
    }

    if (!fileinit(f, w, w->next)) {
        free(f);
        twarnx("OOM");
        return 0;
    }

    filewopen(f);
    if (!f->iswopen) {
        free(f->path);
        free(f);
        return 0;
    }

    w->next++;
    fileadd(f, w);
    return 1;
}


static void
moveresv(File *to, File *from, int n)
{
    from->resv -= n;
    from->free += n;
    to->resv += n;
    to->free -= n;
}


static int
needfree(Wal *w, int n)
{
    if (w->tail->free >= n) return n;
    if (makenextfile(w)) return n;
    return 0;
}


// Ensures:
//  1. b->resv is congruent to n (mod z).
//  2. x->resv is congruent to 0 (mod z) for each future file x.
// Assumes (and preserves) that b->resv >= n.
// Reserved space is conserved (neither created nor destroyed);
// we just move it around to preserve the invariant.
// We might have to allocate a new file.
// Returns 1 on success, otherwise 0. If there was a failure,
// w->tail is not updated.
static int
balancerest(Wal *w, File *b, int n)
{
    int rest, c, r;
    static const int z = sizeof(int) + sizeof(Jobrec);

    if (!b) return 1;

    rest = b->resv - n;
    r = rest % z;
    if (r == 0) return balancerest(w, b->next, 0);

    c = z - r;
    if (w->tail->resv >= c && b->free >= c) {
        moveresv(b, w->tail, c);
        return balancerest(w, b->next, 0);
    }

    if (needfree(w, r) != r) {
        twarnx("needfree");
        return 0;
    }
    moveresv(w->tail, b, r);
    return balancerest(w, b->next, 0);
}


// Ensures:
//  1. w->cur->resv >= n.
//  2. w->cur->resv is congruent to n (mod z).
//  3. x->resv is congruent to 0 (mod z) for each future file x.
// (where z is the size of a delete record in the wal).
// Reserved space is conserved (neither created nor destroyed);
// we just move it around to preserve the invariant.
// We might have to allocate a new file.
// Returns 1 on success, otherwise 0. If there was a failure,
// w->tail is not updated.
static int
balance(Wal *w, int n)
{
    int r;

    // Invariant 1
    // (this loop will run at most once)
    while (w->cur->resv < n) {
        int m = w->cur->resv;

        r = needfree(w, m);
        if (r != m) {
            twarnx("needfree");
            return 0;
        }

        moveresv(w->tail, w->cur, m);
        usenext(w);
    }

    // Invariants 2 and 3
    return balancerest(w, w->cur, n);
}


// Returns the number of bytes successfully reserved: either 0 or n.
static int
reserve(Wal *w, int n)
{
    int r;

    // return value must be nonzero but is otherwise ignored
    if (!w->use) return 1;

    if (w->cur->free >= n) {
        w->cur->free -= n;
        w->cur->resv += n;
        return n;
    }

    r = needfree(w, n);
    if (r != n) {
        twarnx("needfree");
        return 0;
    }

    w->tail->free -= n;
    w->tail->resv += n;
    if (!balance(w, n)) {
        // error; undo the reservation
        w->tail->resv -= n;
        w->tail->free += n;
        return 0;
    }

    return n;
}


// Returns the number of bytes reserved or 0 on error.
int
walresvput(Wal *w, job j)
{
    int z = 0;

    // reserve space for the initial job record
    z += sizeof(int);
    z += strlen(j->tube->name);
    z += sizeof(Jobrec);
    z += j->r.body_size;

    // plus space for a delete to come later
    z += sizeof(int);
    z += sizeof(Jobrec);

    return reserve(w, z);
}


// Returns the number of bytes reserved or 0 on error.
int
walresvupdate(Wal *w, job j)
{
    int z = 0;

    z +=sizeof(int);
    z +=sizeof(Jobrec);
    return reserve(w, z);
}


// Returns the number of locks acquired: either 0 or 1.
int
waldirlock(Wal *w)
{
    int r;
    int fd;
    struct flock lk;
    char path[PATH_MAX];

    r = snprintf(path, PATH_MAX, "%s/lock", w->dir);
    if (r > PATH_MAX) {
        twarnx("path too long: %s/lock", w->dir);
        return 0;
    }

    fd = open(path, O_WRONLY|O_CREAT, 0600);
    if (fd == -1) {
        twarn("open");
        return 0;
    }

    lk.l_type = F_WRLCK;
    lk.l_whence = SEEK_SET;
    lk.l_start = 0;
    lk.l_len = 0;
    r = fcntl(fd, F_SETLK, &lk);
    if (r) {
        twarn("fcntl");
        return 0;
    }

    // intentionally leak fd, since we never want to close it
    // and we'll never need it again
    return 1;
}


void
walread(Wal *w, job list, int min, int max)
{
    File *f;
    int i, fd;
    int err = 0;

    for (i = min; i < w->next; i++) {
        f = new(File);
        if (!f) {
            twarnx("OOM");
            exit(1);
        }

        if (!fileinit(f, w, i)) {
            free(f);
            twarnx("OOM");
            exit(1);
        }

        fd = open(f->path, O_RDONLY);
        if (fd < 0) {
            twarn("%s", f->path);
            free(f->path);
            free(f);
            continue;
        }

        f->fd = fd;
        fileadd(f, w);
        err |= fileread(f, list);
        close(fd);
    }

    if (err) {
        warnx("Errors reading one or more WAL files.");
        warnx("Continuing. You may be missing data.");
    }
}


void
walinit(Wal *w, job list)
{
    int min;

    min = walscandir(w);
    walread(w, list, min, w->next);

    // first writable file
    if (!makenextfile(w)) {
        twarnx("makenextfile");
        exit(1);
    }

    w->cur = w->tail;
}
