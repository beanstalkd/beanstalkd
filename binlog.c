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

#include "tube.h"
#include "job.h"
#include "binlog.h"
#include "util.h"
#include "config.h"

/* max size we will create a log file */
size_t binlog_size_limit = 10 << 20;

char *binlog_dir = NULL;
static int binlog_index = 0;
static int binlog_fd = -1, next_binlog_fd = -1;
static int binlog_version = 2;
static size_t binlog_space = 0, next_binlog_space = 0;
static size_t binlog_reserved = 0, next_binlog_reserved = 0;
static size_t bytes_written;
static int lock_fd;

static binlog first_binlog = NULL, last_binlog = NULL, next_binlog = NULL;

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
binlog_remove_first()
{
    binlog b = first_binlog;

    if (!b) return;

    first_binlog = b->next;
    if (!first_binlog) last_binlog = NULL;

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
        while (first_binlog && first_binlog->refs == 0) binlog_remove_first();
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

#define binlog_warn(fd, path, fmt, args...) \
    warnx("WARNING, " fmt " at %s:%u. %s: ", \
          ##args, path, lseek(fd, 0, SEEK_CUR), \
          "Continuing. You may be missing data.")

static void
binlog_read_one(int fd, job binlog_jobs, const char *path)
{
    struct job js;
    tube t;
    job j;
    char tubename[MAX_TUBE_NAME_LEN];
    size_t namelen;
    ssize_t r;
    int version;

    r = read(fd, &version, sizeof(version));
    if (r == -1) return twarn("read()");
    if (r < sizeof(version)) {
        return binlog_warn(fd, path, "EOF while reading version record");
    }

    if (version != binlog_version) {
        return warnx("%s: binlog version mismatch %d %d", path, version,
                     binlog_version);
    }

    while (read(fd, &namelen, sizeof(size_t)) == sizeof(size_t)) {
        if (namelen > 0) {
            r = read(fd, tubename, namelen);
            if (r == -1) return twarn("read()");
            if (r < namelen) {
                lseek(fd, SEEK_CUR, 0);
                return binlog_warn(fd, path, "EOF while reading tube name");
            }
        }

        tubename[namelen] = '\0';
        r = read(fd, &js, sizeof(struct job));
        if (r == -1) return twarn("read()");
        if (r < sizeof(struct job)) {
          return binlog_warn(fd, path, "EOF while reading job record");
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
            if (!j) {
                t = tube_find_or_make(tubename);
                j = make_job_with_id(js.pri, js.delay, js.ttr, js.body_size,
                                     t, js.id);
                j->next = j->prev = j;
                j->creation = js.creation;
                job_insert(binlog_jobs, j);
            }
            if (js.body_size) {
                if (js.body_size > j->body_size) {
                    warnx("job size increased from %zu to %zu", j->body_size,
                          js.body_size);
                    job_remove(j);
                    binlog_dref(j->binlog);
                    job_free(j);
                    return binlog_warn(fd, path, "EOF while reading job body");
                }
                r = read(fd, j->body, js.body_size);
                if (r == -1) return twarn("read()");
                if (r < js.body_size) {
                    warnx("dropping incomplete job %llu", j->id);
                    job_remove(j);
                    binlog_dref(j->binlog);
                    job_free(j);
                    return binlog_warn(fd, path, "EOF while reading job body");
                }
            }
            break;
        }
        if (j) {
            j->state = js.state;
            j->deadline = js.deadline;
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
                j->binlog = binlog_iref(last_binlog);
            }
        }
    }
}

static void
binlog_close_last()
{
    if (binlog_fd < 0) return;
    close(binlog_fd);
    binlog_dref(last_binlog);
    binlog_fd = -1;
}

static binlog
make_binlog(char *path)
{
    binlog b;

    b = (binlog) malloc(sizeof(struct binlog) + strlen(path) + 1);
    if (!b) return twarnx("OOM"), NULL;
    strcpy(b->path, path);
    b->refs = 0;
    b->next = NULL;
    return b;
}

static binlog
make_next_binlog()
{
    int r;
    char path[PATH_MAX];

    if (!binlog_dir) return NULL;

    r = snprintf(path, PATH_MAX, "%s/binlog.%d", binlog_dir, ++binlog_index);
    if (r > PATH_MAX) return twarnx("path too long: %s", binlog_dir), NULL;

    return make_binlog(path);
}

static binlog
add_binlog(binlog b)
{
    if (last_binlog) last_binlog->next = b;
    last_binlog = b;
    if (!first_binlog) first_binlog = b;

    return b;
}

static int
binlog_open(binlog log)
{
    int fd;

    if (!binlog_iref(log)) return -1;

    fd = open(log->path, O_WRONLY | O_CREAT, 0400);

    if (fd < 0) return twarn("Cannot open binlog %s", log->path), -1;

#ifdef HAVE_POSIX_FALLOCATE
    {
        int r;
        r = posix_fallocate(fd, 0, binlog_size_limit);
        if (r) {
            close(fd);
            binlog_dref(log);
            errno = r;
            return twarn("Cannot allocate space for binlog %s", log->path), -1;
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
                return -1;
            }
        }

        p = lseek(fd, 0, SEEK_SET);
        if (p == -1) {
            twarn("lseek");
            close(fd);
            binlog_dref(log);
            return -1;
        }
    }
#endif

    bytes_written = write(fd, &binlog_version, sizeof(int));

    if (bytes_written < sizeof(int)) {
        twarn("Cannot write to binlog");
        close(fd);
        binlog_dref(log);
        return -1;
    }

    return fd;
}

/* returns 1 on success, 0 on error. */
static int
binlog_use_next()
{
    if (binlog_fd < 0) return 0;
    if (next_binlog_fd < 0) return 0;
    if (binlog_reserved > next_binlog_space) return twarnx("overextended"), 0;

    binlog_close_last();

    binlog_fd = next_binlog_fd;
    add_binlog(next_binlog);

    next_binlog = NULL;
    next_binlog_fd = -1;

    binlog_space = next_binlog_space - binlog_reserved;
    binlog_reserved = next_binlog_reserved + binlog_reserved;

    next_binlog_reserved = next_binlog_space = 0;
    return 1;
}

void
binlog_close()
{
    binlog_use_next();
    binlog_close_last();
}

/* Returns the number of jobs successfully written (either 0 or 1). */
/* If we are not using the binlog at all (binlog_fd < 0), then we pretend to
   have made a successful write and return 1. */
int
binlog_write_job(job j)
{
    ssize_t written;
    size_t tube_namelen, to_write = 0;
    struct iovec vec[4], *vptr;
    int vcnt = 3, r;

    if (binlog_fd < 0) return 1;
    tube_namelen = 0;

    vec[0].iov_base = (char *) &tube_namelen;
    to_write += vec[0].iov_len = sizeof(size_t);

    vec[1].iov_base = j->tube->name;
    vec[1].iov_len = 0;

    /* we could save some bytes in the binlog file by only saving some parts of
     * the job struct */
    vec[2].iov_base = (char *) j;
    to_write += vec[2].iov_len = sizeof(struct job);

    printf("writing job %lld state %d\n", j->id, j->state);

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

    if (to_write > binlog_reserved) {
        r = binlog_use_next();
        if (!r) return twarnx("failed to use next binlog"), 0;
    }

    if (j->state && !j->binlog) j->binlog = binlog_iref(last_binlog);

    while (to_write > 0) {
        written = writev(binlog_fd, vec, vcnt);

        if (written < 0) {
            if (errno == EAGAIN) continue;
            if (errno == EINTR) continue;

            twarn("writev");
            binlog_close_last();
            return 0;
        }

        bytes_written += written;
        to_write -= written;
        if (to_write > 0 && written > 0) {
            for (vptr = vec; written >= vptr->iov_len; vptr++) {
                written -= vptr->iov_len;
                vptr->iov_len = 0;
            }
            vptr->iov_base = (char *) vptr->iov_base + written;
            vptr->iov_len -= written;
        }
    }

    return 1;
}

/* Returns the number of bytes successfully reserved: either 0 or n. */
static size_t
binlog_reserve_space(size_t n)
{
    /* This value must be nonzero but is otherwise ignored. */
    if (binlog_fd < 0) return 1;

    if (n <= binlog_space) {
        binlog_space -= n;
        binlog_reserved += n;
        return n;
    }

    if (n <= next_binlog_space) {
        next_binlog_space -= n;
        next_binlog_reserved += n;
        return n;
    }

    /* The next binlog is already allocated and it is full. */
    if (next_binlog_fd >= 0) return 0;

    /* open a new binlog with more space to reserve */
    next_binlog = make_next_binlog();
    if (!next_binlog) return twarnx("error making next binlog"), 0;
    next_binlog_fd = binlog_open(next_binlog);

    /* open failed, so we can't reserve any space */
    if (next_binlog_fd < 0) return 0;

    next_binlog_space = binlog_size_limit - bytes_written - n;
    next_binlog_reserved = n;

    return n;
}

/* Returns the number of bytes reserved. */
size_t
binlog_reserve_space_put(job j)
{
    size_t z = 0;

    /* reserve space for the initial job record */
    z += sizeof(size_t);
    z += strlen(j->tube->name);
    z += sizeof(struct job);
    z += j->body_size;

    /* plus space for a delete to come later */
    z += sizeof(size_t);
    z += sizeof(struct job);

    return binlog_reserve_space(z);
}

size_t
binlog_reserve_space_update(job j)
{
    size_t z = 0;

    z += sizeof(size_t);
    z += sizeof(struct job);
    return binlog_reserve_space(z);
}

void
binlog_read(job binlog_jobs)
{
    int binlog_index_min;
    struct stat sbuf;
    int fd, idx, r;
    char path[PATH_MAX];
    binlog b;

    if (!binlog_dir) return;

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
                binlog_read_one(fd, binlog_jobs, path);
                close(fd);
                binlog_dref(b);
            }
        }

    }
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
binlog_init()
{
    binlog log;

    if (!binlog_dir) return;

    log = make_next_binlog();
    if (!log) return twarnx("error making first binlog");
    binlog_fd = binlog_open(log);
    if (binlog_fd >= 0) add_binlog(log);
}

const char *
binlog_oldest_index()
{
    if (!first_binlog) return "0";

    return strrchr(first_binlog->path, '.') + 1;
}

const char *
binlog_current_index()
{
    if (!last_binlog) return "0";

    return strrchr(last_binlog->path, '.') + 1;
}
