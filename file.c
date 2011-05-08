#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "dat.h"

static void warnpos(File*, char*, ...);
static int  readrec(File*, job, int*);
static int  readfull(File*, void*, int, int*, char*);


void
fileincref(File *f)
{
    if (!f) return;
    f->refs++;
}


void
filedecref(File *f)
{
    if (!f) return;
    f->refs--;
    if (f->refs < 1) {
        walgc(f->w);
    }
}


// Fileread reads jobs from f->path into list.
// It returns 0 on success, or 1 if any errors occurred.
int
fileread(File *f, job list)
{
    int err = 0, v;

    if (!readfull(f, &v, sizeof(v), &err, "version")) {
        return err;
    }
    if (v != Walver) {
        warnx("%s: wrong version: want %d, got %d", f->path, Walver, v);
        return 1;
    }

    fileincref(f);
    while (readrec(f, list, &err));
    filedecref(f);
    return err;
}


// Readrec reads a record from f->fd into linked list l.
// If an error occurs, it sets *err to 1.
// Readrec returns the number of records read, either 1 or 0.
static int
readrec(File *f, job l, int *err)
{
    int r;
    int namelen;
    Jobrec jr;
    job j;
    tube t;
    char tubename[MAX_TUBE_NAME_LEN];

    r = read(f->fd, &namelen, sizeof(int));
    if (r == -1) {
        twarn("read");
        warnpos(f, "error");
        *err = 1;
        return 0;
    }
    if (r != sizeof(int)) {
        return 0;
    }
    if (namelen >= MAX_TUBE_NAME_LEN) {
        warnpos(f, "namelen %d exceeds maximum of %d", namelen, MAX_TUBE_NAME_LEN - 1);
        *err = 1;
        return 0;
    }

    if (namelen) {
        if (!readfull(f, tubename, namelen, err, "tube name")) {
            return 0;
        }
    }
    tubename[namelen] = '\0';

    if (!readfull(f, &jr, sizeof(Jobrec), err, "job struct")) {
        return 0;
    }

    // are we reading trailing zeroes?
    if (!jr.id) return 0;

    j = job_find(jr.id);
    if (!(j || namelen)) {
        // We read a short record without having seen a
        // full record for this job, so the full record
        // was in an eariler file that has been deleted.
        // Therefore the job itself has either been
        // deleted or migrated; either way, this record
        // should be ignored.
        return 1;
    }

    switch (jr.state) {
    case Reserved:
        jr.state = Ready;
    case Ready:
    case Buried:
    case Delayed:
        if (!j) {
            t = tube_find_or_make(tubename);
            j = make_job_with_id(jr.pri, jr.delay, jr.ttr, jr.body_size,
                                 t, jr.id);
            j->next = j->prev = j;
            j->r.created_at = jr.created_at;
        }
        j->r = jr;
        job_insert(l, j);

        // full record; read the job body
        if (namelen) {
            if (jr.body_size != j->r.body_size) {
                warnpos(f, "job %llu size changed", j->r.id);
                warnpos(f, "was %zu, now %zu", j->r.body_size, jr.body_size);
                goto Error;
            }
            if (!readfull(f, j->body, j->r.body_size, err, "job body")) {
                goto Error;
            }

            // since this is a full record, we can move
            // the file pointer and decref the old
            // file, if any
            filedecref(j->file);
            j->file = f;
            fileincref(j->file);
        }

        return 1;
    case Invalid:
        if (j) {
            job_remove(j);
            filedecref(j->file);
            job_free(j);
        }
        return 1;
    }

Error:
    *err = 1;
    if (j) {
        job_remove(j);
        filedecref(j->file);
        job_free(j);
    }
    return 0;
}


static int
readfull(File *f, void *c, int n, int *err, char *desc)
{
    int r;

    r = read(f->fd, c, n);
    if (r == -1) {
        twarn("read");
        warnpos(f, "error reading %s", desc);
        *err = 1;
        return 0;
    }
    if (r != n) {
        warnpos(f, "unexpected EOF reading %s", desc);
        *err = 1;
        return 0;
    }
    return r;
}


static void
warnpos(File *f, char *fmt, ...)
{
    int off;
    va_list ap;

    off = lseek(f->fd, 0, SEEK_CUR);
    fprintf(stderr, "%s:%u: ", f->path, off);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}


// Opens f for writing, writes a header, and initializes
// f->free and f->resv.
// Sets f->iswopen if successful.
void
filewopen(File *f)
{
    int fd, r;
    int n;
    int ver = Walver;

    fd = open(f->path, O_WRONLY|O_CREAT, 0400);
    if (fd < 0) {
        twarn("open %s", f->path);
        return;
    }

    r = falloc(fd, f->w->filesz);
    if (r) {
        close(fd);
        errno = r;
        twarn("Cannot allocate space for file %s", f->path);
        return;
    }

    n = write(fd, &ver, sizeof(int));
    if (n < sizeof(int)) {
        twarn("write %s", f->path);
        close(fd);
        return;
    }

    f->fd = fd;
    f->iswopen = 1;
    fileincref(f);
    f->free = f->w->filesz - n;
    f->resv = 0;
}


static int
filewrite(File *f, job j, void *buf, int len)
{
    int r;

    r = write(f->fd, buf, len);
    if (r != len) {
        twarn("write");
        return 0;
    }

    f->resv -= r;
    j->walresv -= r;
    return 1;
}


int
filewrjobshort(File *f, job j)
{
    int nl;

    if (j->r.state == Invalid) {
        filedecref(j->file);
        j->file = NULL;
    }
    nl = 0; // name len 0 indicates short record
    return
        filewrite(f, j, &nl, sizeof nl) &&
        filewrite(f, j, &j->r, sizeof j->r);
}


int
filewrjobfull(File *f, job j)
{
    int nl;

    j->file = f;
    fileincref(j->file);
    nl = strlen(j->tube->name);
    return
        filewrite(f, j, &nl, sizeof nl) &&
        filewrite(f, j, j->tube->name, nl) &&
        filewrite(f, j, &j->r, sizeof j->r) &&
        filewrite(f, j, j->body, j->r.body_size);
}


void
filewclose(File *f)
{
    int r;

    if (!f) return;
    if (!f->iswopen) return;
    if (f->free) {
        // Some compilers give a warning if the return value of ftruncate is
        // ignored. So we pretend to use it.
        r = ftruncate(f->fd, f->w->filesz - f->free);
        if (r == -1); // do nothing
    }
    close(f->fd);
    f->iswopen = 0;
    filedecref(f);
}


int
fileinit(File *f, Wal *w, int n)
{
    f->w = w;
    f->seq = n;
    f->path = fmtalloc("%s/binlog.%d", w->dir, n);
    return !!f->path;
}


// Adds f to the linked list in w,
// updating w->tail and w->head as necessary.
Wal*
fileadd(File *f, Wal *w)
{
    if (w->tail) {
        w->tail->next = f;
    }
    w->tail = f;
    if (!w->head) {
        w->head = f;
    }
    return w;
}
