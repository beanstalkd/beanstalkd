/* binlog.c - binary log implementation */

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

#include "config.h"

#if HAVE_STDINT_H
# include <stdint.h>
#endif /* else we get int types from config.h */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <limits.h>
#include <stddef.h>

#include "tube.h"
#include "job.h"
#include "binlog.h"
#include "util.h"
#include "port.h"

typedef struct binlog *binlog;

struct binlog {
  binlog next;
  unsigned int refs;
  int fd;
  size_t free;
  size_t reserved;
  char path[];
};

/* max size we will create a log file */
size_t binlog_size_limit = BINLOG_SIZE_LIMIT_DEFAULT;

int enable_fsync = 0;
size_t fsync_throttle_ms = 0;
uint64_t last_fsync = 0;

char *binlog_dir = NULL;
static int binlog_index = 0;
static int binlog_version = 5;
static int lock_fd;

static binlog oldest_binlog = 0,
              current_binlog = 0,
              newest_binlog = 0;

static const size_t job_record_size = offsetof(struct job, pad);

static int
binlog_scan_dir()
{
    DIR *dirp;
    struct dirent *dp;
    long min = 0;
    long max = 0;
    long val;
    char *endptr;
    size_t name_len;

    dirp = opendir(binlog_dir);
    if (!dirp) return 0;

    while ((dp = readdir(dirp)) != NULL) {
        name_len = strlen(dp->d_name);
        if (name_len > 7 && !strncmp("binlog.", dp->d_name, 7)) {
            val = strtol(dp->d_name + 7, &endptr, 10);
            if (endptr && *endptr == 0) {
                if (max == 0 || val > max) max = val;
                if (min == 0 || val < min) min = val;
            }
        }
    }

    closedir(dirp);
    binlog_index = (int) max;
    return (int) min;
}

static void
binlog_remove_oldest()
{
    binlog b = oldest_binlog;

    if (!b) return;

    oldest_binlog = b->next;

    unlink(b->path);
    free(b);
}

static binlog
binlog_iref(binlog b)
{
    if (b) b->refs++;
    return b;
}

static void
binlog_dref(binlog b)
{
    if (!b) return;
    if (b->refs < 1) return twarnx("refs is zero for binlog: %s", b->path);

    --b->refs;
    if (b->refs < 1) {
        while (oldest_binlog && oldest_binlog->refs == 0) {
            binlog_remove_oldest();
        }
    }
}

/*
static void
binlog_warn(int fd, const char* path, const char *msg)
{
    warnx("WARNING, %s at %s:%u.\n%s", msg, path, lseek(fd, 0, SEEK_CUR),
          "  Continuing. You may be missing data.");
}
*/

#define binlog_warn(b, fmt, args...) \
    warnx("WARNING, " fmt " at %s:%u. %s: ", \
          ##args, b->path, lseek(b->fd, 0, SEEK_CUR), \
          "Continuing. You may be missing data.")

static void
binlog_read_log_file(binlog b, job binlog_jobs)
{
    struct job js;
    tube t;
    job j;
    char tubename[MAX_TUBE_NAME_LEN];
    size_t namelen;
    ssize_t r;
    int version;

    r = read(b->fd, &version, sizeof(version));
    if (r == -1) return twarn("read()");
    if (r < sizeof(version)) {
        return binlog_warn(b, "EOF while reading version record");
    }

    if (version != binlog_version) {
        return warnx("%s: binlog version mismatch %d %d", b->path, version,
                     binlog_version);
    }

    while (read(b->fd, &namelen, sizeof(size_t)) == sizeof(size_t)) {
        if (namelen > 0) {
            r = read(b->fd, tubename, namelen);
            if (r == -1) return twarn("read()");
            if (r < namelen) {
                lseek(b->fd, SEEK_CUR, 0);
                return binlog_warn(b, "EOF while reading tube name");
            }
        }

        tubename[namelen] = '\0';
        r = read(b->fd, &js, job_record_size);
        if (r == -1) return twarn("read()");
        if (r < job_record_size) {
          return binlog_warn(b, "EOF while reading job record");
        }

        if (!js.id) break;

        j = job_find(js.id);
        switch (js.state) {
        case JOB_STATE_INVALID:
            if (j) {
                job_remove(j);
                binlog_dref(j->binlog);
                job_free(j);
                j = NULL;
            }
            break;
        case JOB_STATE_READY:
        case JOB_STATE_DELAYED:
            if (!j && namelen > 0) {
                t = tube_find_or_make(tubename);
                j = make_job_with_id(js.pri, js.delay, js.ttr, js.body_size,
                                     t, js.id);
                j->next = j->prev = j;
                j->created_at = js.created_at;
                job_insert(binlog_jobs, j);
            }
            if (js.body_size) {
                if (js.body_size > j->body_size) {
                    warnx("job size increased from %zu to %zu", j->body_size,
                          js.body_size);
                    job_remove(j);
                    binlog_dref(j->binlog);
                    job_free(j);
                    return binlog_warn(b, "EOF while reading job body");
                }
                r = read(b->fd, j->body, js.body_size);
                if (r == -1) return twarn("read()");
                if (r < js.body_size) {
                    warnx("dropping incomplete job %llu", j->id);
                    job_remove(j);
                    binlog_dref(j->binlog);
                    job_free(j);
                    return binlog_warn(b, "EOF while reading job body");
                }
            }
            break;
        }
        if (j) {
            j->state = js.state;
            j->deadline_at = js.deadline_at;
            j->pri = js.pri;
            j->delay = js.delay;
            j->ttr = js.ttr;
            j->timeout_ct = js.timeout_ct;
            j->release_ct = js.release_ct;
            j->bury_ct = js.bury_ct;
            j->kick_ct = js.kick_ct;

            /* this is a complete record, so we can move the binlog ref */
            if (namelen && js.body_size) {
                binlog_dref(j->binlog);
                j->binlog = binlog_iref(b);
            }
        }
    }
}

static void
binlog_close(binlog b)
{
    if (!b) return;
    if (b->fd < 0) return;
    close(b->fd);
    b->fd = -1;
    binlog_dref(b);
}

static binlog
make_binlog(char *path)
{
    binlog b;

    b = (binlog) malloc(sizeof(struct binlog) + strlen(path) + 1);
    if (!b) return twarnx("OOM"), (binlog) 0;
    strcpy(b->path, path);
    b->refs = 0;
    b->next = NULL;
    b->fd = -1;
    b->free = 0;
    b->reserved = 0;
    return b;
}

static binlog
make_next_binlog()
{
    int r;
    char path[PATH_MAX];

    if (!binlog_dir) return NULL;

    r = snprintf(path, PATH_MAX, "%s/binlog.%d", binlog_dir, ++binlog_index);
    if (r > PATH_MAX) return twarnx("path too long: %s", binlog_dir), (binlog)0;

    return make_binlog(path);
}

static binlog
add_binlog(binlog b)
{
    if (newest_binlog) newest_binlog->next = b;
    newest_binlog = b;
    if (!oldest_binlog) oldest_binlog = b;

    return b;
}

static void
binlog_open(binlog log, size_t *written)
{
    int fd;
    size_t bytes_written;

    if (written) *written = 0;

    if (!binlog_iref(log)) return;

    fd = open(log->path, O_WRONLY | O_CREAT, 0400);

    if (fd < 0) return twarn("Cannot open binlog %s", log->path);

#ifdef HAVE_POSIX_FALLOCATE
    {
        int r;
        r = posix_fallocate(fd, 0, binlog_size_limit);
        if (r) {
            close(fd);
            binlog_dref(log);
            errno = r;
            return twarn("Cannot allocate space for binlog %s", log->path);
        }
    }
#else
    /* Allocate space in a slow but portable way. */
    {
        size_t i;
        ssize_t w;
        off_t p;
        #define ZERO_BUF_SIZE 512
        char buf[ZERO_BUF_SIZE] = {}; /* initialize to zero */

        for (i = 0; i < binlog_size_limit; i += w) {
            w = write(fd, &buf, ZERO_BUF_SIZE);
            if (w == -1) {
                twarn("Cannot allocate space for binlog %s", log->path);
                close(fd);
                binlog_dref(log);
                return;
            }
        }

        p = lseek(fd, 0, SEEK_SET);
        if (p == -1) {
            twarn("lseek");
            close(fd);
            binlog_dref(log);
            return;
        }
    }
#endif

    bytes_written = write(fd, &binlog_version, sizeof(int));
    if (written) *written = bytes_written;

    if (bytes_written < sizeof(int)) {
        twarn("Cannot write to binlog");
        close(fd);
        binlog_dref(log);
        return;
    }

    log->fd = fd;
}

/* returns 1 on success, 0 on error. */
static int
binlog_use_next()
{
    binlog next;

    if (!current_binlog) return 0;

    next = current_binlog->next;

    if (!next) return 0;

    /* assert(current_binlog->reserved == 0); */

    binlog_close(current_binlog);
    current_binlog = next;

    return 1;
}

void
binlog_shutdown()
{
    binlog_use_next();
    binlog_close(current_binlog);
}

/* Returns the number of jobs successfully written (either 0 or 1).

   If this fails, something is seriously wrong. It should never fail because of
   a full disk. (The binlog_reserve_space_* functions, on the other hand, can
   fail because of a full disk.)

   If we are not using the binlog at all (!current_binlog), then we pretend to
   have made a successful write and return 1. */
int
binlog_write_job(job j)
{
    ssize_t written;
    size_t tube_namelen, to_write = 0;
    struct iovec vec[4], *vptr;
    int vcnt = 3, r;
    uint64_t now;

    if (!current_binlog) return 1;
    tube_namelen = 0;

    vec[0].iov_base = (char *) &tube_namelen;
    to_write += vec[0].iov_len = sizeof(size_t);

    vec[1].iov_base = j->tube->name;
    vec[1].iov_len = 0;

    vec[2].iov_base = (char *) j;
    to_write += vec[2].iov_len = job_record_size;

    if (j->state == JOB_STATE_READY || j->state == JOB_STATE_DELAYED) {
        if (!j->binlog) {
            tube_namelen = strlen(j->tube->name);
            to_write += vec[1].iov_len = tube_namelen;
            vcnt = 4;
            vec[3].iov_base = j->body;
            to_write += vec[3].iov_len = j->body_size;
        }
    } else if (j->state == JOB_STATE_INVALID) {
        if (j->binlog) binlog_dref(j->binlog);
        j->binlog = NULL;
    } else {
        return twarnx("unserializable job state: %d", j->state), 0;
    }

    if (to_write > current_binlog->reserved) {
        r = binlog_use_next();
        if (!r) return twarnx("failed to use next binlog"), 0;
    }

    if (j->state && !j->binlog) j->binlog = binlog_iref(current_binlog);

    while (to_write > 0) {
        written = writev(current_binlog->fd, vec, vcnt);

        if (written < 0) {
            if (errno == EAGAIN) continue;
            if (errno == EINTR) continue;

            twarn("writev");
            binlog_close(current_binlog);
            current_binlog = 0;
            return 0;
        }

        to_write -= written;
        if (to_write > 0 && written > 0) {
            for (vptr = vec; written >= vptr->iov_len; vptr++) {
                written -= vptr->iov_len;
                vptr->iov_len = 0;
            }
            vptr->iov_base = (char *) vptr->iov_base + written;
            vptr->iov_len -= written;
        }
        current_binlog->reserved -= written;
        j->reserved_binlog_space -= written;
    }

    now = now_usec() / 1000; /* usec -> msec */
    if (enable_fsync && now - last_fsync >= fsync_throttle_ms) {
        r = fdatasync(current_binlog->fd);
        if (r == -1) return twarn("fdatasync"), 0;
        last_fsync = now;
    }

    return 1;
}

static binlog
make_future_binlog()
{
    binlog b;
    size_t header;

    /* open a new binlog with more space to reserve */
    b = make_next_binlog();
    if (!b) return twarnx("error making next binlog"), (binlog) 0;
    binlog_open(b, &header);

    /* open failed, so we can't reserve any space */
    if (b->fd < 0) {
        free(b);
        return 0;
    }

    b->free = binlog_size_limit - header;
    b->reserved = 0;
    return b;
}

static int
can_move_reserved(size_t n, binlog from, binlog to)
{
    return from->reserved >= n && to->free >= n;
}

static void
move_reserved(size_t n, binlog from, binlog to)
{
    from->reserved -= n;
    from->free += n;
    to->reserved += n;
    to->free -= n;
}

static size_t
ensure_free_space(size_t n)
{
    binlog fb;

    if (newest_binlog && newest_binlog->free >= n) return n;

    /* open a new binlog */
    fb = make_future_binlog();
    if (!fb) return twarnx("make_future_binlog"), 0;

    add_binlog(fb);
    return n;
}

/* Preserve some invariants immediately after any space reservation.
 * Invariant 1: current_binlog->reserved >= n.
 * Invariant 2: current_binlog->reserved is congruent to n (mod z), where z
 * is the size of a delete record in the binlog. */
static size_t
maintain_invariant(size_t n)
{
    size_t reserved_later, remainder, complement, z, r;

    /* In this function, reserved bytes are conserved (they are neither created
     * nor destroyed). We just move them around to preserve the invariant. We
     * might have to create new free space (i.e. allocate a new binlog file),
     * though. */

    /* Invariant 1. */
    /* This is a loop, but it's guaranteed to run at most once. The proof is
     * left as an exercise for the reader. */
    while (current_binlog->reserved < n) {
        size_t to_move = current_binlog->reserved;

        r = ensure_free_space(to_move);
        if (r != to_move) {
            twarnx("ensure_free_space");
            if (newest_binlog->reserved >= n) {
                newest_binlog->reserved -= n;
            } else {
                twarnx("failed to unreserve %zd bytes", n); /* can't happen */
            }
            return 0;
        }

        move_reserved(to_move, current_binlog, newest_binlog);
        binlog_use_next();
    }


    /* Invariant 2. */

    z = sizeof(size_t) + job_record_size;
    reserved_later = current_binlog->reserved - n;
    remainder = reserved_later % z;
    if (remainder == 0) return n;
    complement = z - remainder;
    if (can_move_reserved(complement, newest_binlog, current_binlog)) {
        move_reserved(complement, newest_binlog, current_binlog);
        return n;
    }

    r = ensure_free_space(remainder);
    if (r != remainder) {
        twarnx("ensure_free_space");
        if (newest_binlog->reserved >= n) {
            newest_binlog->reserved -= n;
        } else {
            twarnx("failed to unreserve %zd bytes", n); /* can't happen */
        }
        return 0;
    }
    move_reserved(remainder, current_binlog, newest_binlog);

    return n;
}

/* Returns the number of bytes successfully reserved: either 0 or n. */
static size_t
binlog_reserve_space(size_t n)
{
    size_t r;

    /* This return value must be nonzero but is otherwise ignored. */
    if (!current_binlog) return 1;

    if (current_binlog->free >= n) {
        current_binlog->free -= n;
        current_binlog->reserved += n;
        return maintain_invariant(n);
    }

    r = ensure_free_space(n);
    if (r != n) return twarnx("ensure_free_space"), 0;

    newest_binlog->free -= n;
    newest_binlog->reserved += n;
    return maintain_invariant(n);
}

/* Returns the number of bytes reserved. */
size_t
binlog_reserve_space_put(job j)
{
    size_t z = 0;

    /* reserve space for the initial job record */
    z += sizeof(size_t);
    z += strlen(j->tube->name);
    z += job_record_size;
    z += j->body_size;

    /* plus space for a delete to come later */
    z += sizeof(size_t);
    z += job_record_size;

    return binlog_reserve_space(z);
}

size_t
binlog_reserve_space_update(job j)
{
    size_t z = 0;

    z += sizeof(size_t);
    z += job_record_size;
    return binlog_reserve_space(z);
}

int
binlog_lock()
{
    int r;
    struct flock lock;
    char path[PATH_MAX];

    r = snprintf(path, PATH_MAX, "%s/lock", binlog_dir);
    if (r > PATH_MAX) return twarnx("path too long: %s", binlog_dir), 0;

    lock_fd = open(path, O_WRONLY|O_CREAT, 0600);
    if (lock_fd == -1) return twarn("open"), 0;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    r = fcntl(lock_fd, F_SETLK, &lock);
    if (r) return twarn("fcntl"), 0;

    return 1;
}

void
binlog_init(job binlog_jobs)
{
    int binlog_index_min;
    struct stat sbuf;
    int fd, idx, r;
    size_t n;
    char path[PATH_MAX];
    binlog b;

    if (!binlog_dir) return;

    /* Recover any jobs in old binlogs */

    if (stat(binlog_dir, &sbuf) < 0) {
        if (mkdir(binlog_dir, 0700) < 0) return twarn("%s", binlog_dir);
    } else if (!(sbuf.st_mode & S_IFDIR)) {
        twarnx("%s", binlog_dir);
        return;
    }

    binlog_index_min = binlog_scan_dir();

    if (binlog_index_min) {
        for (idx = binlog_index_min; idx <= binlog_index; idx++) {
            r = snprintf(path, PATH_MAX, "%s/binlog.%d", binlog_dir, idx);
            if (r > PATH_MAX) return twarnx("path too long: %s", binlog_dir);

            fd = open(path, O_RDONLY);

            if (fd < 0) {
                twarn("%s", path);
            } else {
                b = binlog_iref(add_binlog(make_binlog(path)));
                b->fd = fd;
                binlog_read_log_file(b, binlog_jobs);
                close(fd);
                b->fd = -1;
                binlog_dref(b);
            }
        }

    }


    /* Set up for writing out new jobs */
    n = ensure_free_space(1);
    if (!n) return twarnx("error making first writable binlog");

    current_binlog = newest_binlog;
}

const char *
binlog_oldest_index()
{
    if (!oldest_binlog) return "0";

    return strrchr(oldest_binlog->path, '.') + 1;
}

const char *
binlog_current_index()
{
    if (!newest_binlog) return "0";

    return strrchr(newest_binlog->path, '.') + 1;
}
