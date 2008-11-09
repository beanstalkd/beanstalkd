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

#include "tube.h"
#include "job.h"
#include "binlog.h"
#include "util.h"
#include "version.h"

/* max size we will create a log file */
size_t binlog_size_limit = 10 << 20;

char *binlog_dir = NULL;
static int binlog_index = 0;
static int binlog_fd = -1;
static int binlog_version = 1;
static size_t bytes_written;

static binlog first_binlog = NULL, last_binlog = NULL;

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

static void
binlog_replay(int fd, job binlog_jobs)
{
    struct job js;
    tube t;
    job j;
    char tubename[MAX_TUBE_NAME_LEN];
    size_t namelen;
    int version;

    if (read(fd, &version, sizeof(version)) < sizeof(version)) {
        return twarn("read()");
    }
    if (version != binlog_version) {
        return twarnx("binlog version mismatch %d %d", version, binlog_version);
    }

    while (read(fd, &namelen, sizeof(size_t)) == sizeof(size_t)) {
        if (namelen > 0 && read(fd, tubename, namelen) != namelen) {
            return twarnx("oops %x %d", namelen, (int)lseek(fd, SEEK_CUR, 0));
        }

        tubename[namelen] = '\0';
        if (read(fd, &js, sizeof(struct job)) != sizeof(struct job)) {
            return twarn("read()");
        }

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
                if (read(fd, j->body, js.body_size) < js.body_size) {
                    twarn("read()");
                    return;
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

void
binlog_close()
{
    if (binlog_fd < 0) return;
    close(binlog_fd);
    binlog_dref(last_binlog);
    binlog_fd = -1;
}

static binlog
add_binlog(char *path)
{
    binlog b;

    b = (binlog)malloc(sizeof(struct binlog) + strlen(path) + 1);
    if (!b) return twarnx("OOM"), NULL;
    strcpy(b->path, path);
    b->refs = 0;
    b->next = NULL;
    if (last_binlog) last_binlog->next = b;
    last_binlog = b;
    if (!first_binlog) first_binlog = b;

    return b;
}

static int
binlog_open()
{
    char path[PATH_MAX];
    binlog b;
    int fd, r;

    if (!binlog_dir) return -1;
    r = snprintf(path, PATH_MAX, "%s/binlog.%d", binlog_dir, ++binlog_index);
    if (r > PATH_MAX) return twarnx("path too long: %s", binlog_dir), -1;

    if (!binlog_iref(add_binlog(path))) return -1;
    fd = open(path, O_WRONLY | O_CREAT, 0400);

    if (fd < 0) {
        twarn("Cannot open binlog %s", path);
        return -1;
    }


    bytes_written = write(fd, &binlog_version, sizeof(int));

    if (bytes_written < sizeof(int)) {
        twarn("Cannot write to binlog");
        close(fd);
        binlog_dref(last_binlog);
        return -1;
    }

    return fd;
}

static void
binlog_open_next()
{
    if (binlog_fd < 0) return;
    close(binlog_fd);
    binlog_dref(last_binlog);
    binlog_fd = binlog_open();
}

void
binlog_write_job(job j)
{
    size_t tube_namelen, to_write;
    struct iovec vec[4], *vptr;
    int vcnt = 3;

    if (binlog_fd < 0) return;
    tube_namelen = 0;

    vec[0].iov_base = (char *) &tube_namelen;
    vec[0].iov_len = sizeof(size_t);
    to_write = sizeof(size_t);

    vec[1].iov_base = j->tube->name;
    vec[1].iov_len = 0;

    /* we could save some bytes in the binlog file by only saving some parts of
     * the job struct */
    vec[2].iov_base = (char *) j;
    vec[2].iov_len = sizeof(struct job);
    to_write += sizeof(struct job);

    if (j->state == JOB_STATE_READY || j->state == JOB_STATE_DELAYED) {
        if (!j->binlog) {
            tube_namelen = strlen(j->tube->name);
            vec[1].iov_len = tube_namelen;
            to_write += tube_namelen;
            j->binlog = binlog_iref(last_binlog);
            vcnt = 4;
            vec[3].iov_base = j->body;
            vec[3].iov_len = j->body_size;
            to_write += j->body_size;
        }
    } else if (j->state == JOB_STATE_INVALID) {
        if (j->binlog) binlog_dref(j->binlog);
        j->binlog = NULL;
    } else {
        return twarnx("unserializable job state: %d", j->state);
    }

    if ((bytes_written + to_write) > binlog_size_limit) binlog_open_next();
    if (binlog_fd < 0) return;

    while (to_write > 0) {
        size_t written = writev(binlog_fd, vec, vcnt);

        if (written < 0) {
            twarn("Cannot write to binlog");
            binlog_close();
            return;
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
                b = binlog_iref(add_binlog(path));
                binlog_replay(fd, binlog_jobs);
                close(fd);
                binlog_dref(b);
            }
        }

    }
}

void
binlog_init()
{
    binlog_fd = binlog_open();
}

