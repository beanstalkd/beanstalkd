// CT - simple-minded unit testing for C

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>
#include "internal.h"
#include "ct.h"


static char *curdir;
static int rjobfd = -1, wjobfd = -1;
static int64 bstart, bdur;
static int btiming; // bool
static int64 bbytes;
static const int64 Second = 1000 * 1000 * 1000;
static const int64 BenchTime = Second;
static const int MaxN = 1000 * 1000 * 1000;



#ifdef __MACH__
#	include <mach/mach_time.h>

static int64
nstime()
{
    return (int64)mach_absolute_time();
}

#else

static int64
nstime()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64)(t.tv_sec)*Second + t.tv_nsec;
}

#endif

void
ctlogpn(char *p, int n, char *fmt, ...)
{
    va_list arg;

    printf("%s:%d: ", p, n);
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);
    putchar('\n');
}


void
ctfail(void)
{
    fflush(stdout);
    fflush(stderr);
    abort();
}


char *
ctdir(void)
{
    mkdir(curdir, 0700);
    return curdir;
}


void
ctresettimer(void)
{
    bdur = 0;
    bstart = nstime();
}


void
ctstarttimer(void)
{
    if (!btiming) {
        bstart = nstime();
        btiming = 1;
    }
}


void
ctstoptimer(void)
{
    if (btiming) {
        bdur += nstime() - bstart;
        btiming = 0;
    }
}


void
ctsetbytes(int n)
{
    bbytes = (int64)n;
}


static void
die(int code, int err, char *msg)
{
    putc('\n', stderr);

    if (msg && *msg) {
        fputs(msg, stderr);
        fputs(": ", stderr);
    }

    fputs(strerror(err), stderr);
    putc('\n', stderr);
    exit(code);
}


static int
tmpfd(void)
{
    FILE *f = tmpfile();
    if (!f) {
        die(1, errno, "tmpfile");
    }
    return fileno(f);
}


static int
failed(int s)
{
    return WIFSIGNALED(s) && (WTERMSIG(s) == SIGABRT);
}


static void
waittest(Test *ts)
{
    Test *t;
    int pid, stat;

    pid = wait3(&stat, 0, 0);
    if (pid == -1) {
        die(3, errno, "wait");
    }
    killpg(pid, 9);

    for (t=ts; t->f; t++) {
        if (t->pid == pid) {
            t->status = stat;
            if (!t->status) {
                putchar('.');
            } else if (failed(t->status)) {
                putchar('F');
            } else {
                putchar('E');
            }
            fflush(stdout);
        }
    }
}


static void
start(Test *t)
{
    t->fd = tmpfd();
    strcpy(t->dir, TmpDirPat);
    mktemp(t->dir);
    t->pid = fork();
    if (t->pid < 0) {
        die(1, errno, "fork");
    } else if (!t->pid) {
        setpgid(0, 0);
        if (dup2(t->fd, 1) == -1) {
            die(3, errno, "dup2");
        }
        if (close(t->fd) == -1) {
            die(3, errno, "fclose");
        }
        if (dup2(1, 2) == -1) {
            die(3, errno, "dup2");
        }
        curdir = t->dir;
        t->f();
        _exit(0);
    }
    setpgid(t->pid, t->pid);
}


static void
runalltest(Test *ts, int limit)
{
    int nrun = 0;
    Test *t;
    for (t=ts; t->f; t++) {
        if (nrun >= limit) {
            waittest(ts);
            nrun--;
        }
        start(t);
        nrun++;
    }
    for (; nrun; nrun--) {
        waittest(ts);
    }
}


static void
copyfd(FILE *out, int in)
{
    ssize_t n;
    char buf[1024]; // arbitrary size

    while ((n = read(in, buf, sizeof(buf))) != 0) {
        if (fwrite(buf, 1, n, out) != (size_t)n) {
            die(3, errno, "fwrite");
        }
    }
}


// Removes path and all of its children.
// Writes errors to stderr and keeps going.
// If path doesn't exist, rmtree returns silently.
static void
rmtree(char *path)
{
    int r = unlink(path);
    if (r == 0 || errno == ENOENT) {
        return; // success
    }
    int unlinkerr = errno;

    DIR *d = opendir(path);
    if (!d) {
        if (errno == ENOTDIR) {
            fprintf(stderr, "ct: unlink: %s\n", strerror(unlinkerr));
        } else {
            perror("ct: opendir");
        }
        fprintf(stderr, "ct: path %s\n", path);
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        int n = strlen(path) + 1 + strlen(ent->d_name);
        char s[n+1];
        sprintf(s, "%s/%s", path, ent->d_name);
        rmtree(s);
    }
    closedir(d);
    r = rmdir(path);
    if (r == -1) {
        perror("ct: rmdir");
        fprintf(stderr, "ct: path %s\n", path);
    }
}


static void
runbenchn(Benchmark *b, int n)
{
    int outfd = tmpfd();
    int durfd = tmpfd();
    strcpy(b->dir, TmpDirPat);
    mktemp(b->dir);
    int pid = fork();
    if (pid < 0) {
        die(1, errno, "fork");
    } else if (!pid) {
        setpgid(0, 0);
        if (dup2(outfd, 1) == -1) {
            die(3, errno, "dup2");
        }
        if (close(outfd) == -1) {
            die(3, errno, "fclose");
        }
        if (dup2(1, 2) == -1) {
            die(3, errno, "dup2");
        }
        curdir = b->dir;
        ctstarttimer();
        b->f(n);
        ctstoptimer();
        write(durfd, &bdur, sizeof bdur);
        write(durfd, &bbytes, sizeof bbytes);
        _exit(0);
    }
    setpgid(pid, pid);

    pid = waitpid(pid, &b->status, 0);
    if (pid == -1) {
        die(3, errno, "wait");
    }
    killpg(pid, 9);
    rmtree(b->dir);
    if (b->status != 0) {
        putchar('\n');
        lseek(outfd, 0, SEEK_SET);
        copyfd(stdout, outfd);
        return;
    }

    lseek(durfd, 0, SEEK_SET);
    int r = read(durfd, &b->dur, sizeof b->dur);
    if (r != sizeof b->dur) {
        perror("read");
        b->status = 1;
    }
    r = read(durfd, &b->bytes, sizeof b->bytes);
    if (r != sizeof b->bytes) {
        perror("read");
        b->status = 1;
    }
}


// rounddown10 rounds a number down to the nearest power of 10.
static int
rounddown10(int n)
{
    int tens = 0;
    // tens = floor(log_10(n))
    while (n >= 10) {
        n = n / 10;
        tens++;
    }
    // result = 10**tens
    int i, result = 1;
    for (i = 0; i < tens; i++) {
        result *= 10;
    }
    return result;
}


// roundup rounds n up to a number of the form [1eX, 2eX, 5eX].
static int
roundup(int n)
{
    int base = rounddown10(n);
    if (n == base)
        return n;
    if (n <= 2*base)
        return 2 * base;
    if (n <= 5*base)
        return 5 * base;
    return 10 * base;
}


static int
min(int a, int b)
{
    if (a < b) {
        return a;
    }
    return b;
}


static int
max(int a, int b)
{
    if (a > b) {
        return a;
    }
    return b;
}


static void
runbench(Benchmark *b)
{
    printf("%s\t", b->name);
    fflush(stdout);
    int n = 1;
    runbenchn(b, n);
    while (b->status == 0 && b->dur < BenchTime && n < MaxN) {
        int last = n;
        // Predict iterations/sec.
        int nsop = b->dur / n;
        if (nsop == 0) {
            n = MaxN;
        } else {
            n = BenchTime / nsop;
        }
        // Run more iterations than we think we'll need for a second (1.5x).
        // Don't grow too fast in case we had timing errors previously.
        // Be sure to run at least one more than last time.
        n = max(min(n+n/2, 100*last), last+1);
        // Round up to something easy to read.
        n = roundup(n);
        runbenchn(b, n);
    }
    if (b->status == 0) {
        printf("%8d\t%10lld ns/op", n, b->dur/n);
        if (b->bytes > 0) {
            double mbs = 0;
            if (b->dur > 0) {
                int64 sec = b->dur / 1000L / 1000L / 1000L;
                int64 nsec = b->dur % 1000000000L;
                double dur = (double)sec + (double)nsec*.0000000001;
                mbs = ((double)b->bytes * (double)n / 1000000) / dur;
            }
            printf("\t%7.2f MB/s", mbs);
        }
        putchar('\n');
    } else {
        if (failed(b->status)) {
            printf("failure");
        } else {
            printf("error");
            if (WIFEXITED(b->status)) {
                printf(" (exit status %d)", WEXITSTATUS(b->status));
            }
            if (WIFSIGNALED(b->status)) {
                printf(" (signal %d)", WTERMSIG(b->status));
            }
        }
        putchar('\n');
    }
}


static void
runallbench(Benchmark *b)
{
    for (; b->f; b++) {
        runbench(b);
    }
}


static int
report(Test *t)
{
    int nfail = 0, nerr = 0;

    putchar('\n');
    for (; t->f; t++) {
        rmtree(t->dir);
        if (!t->status) {
            continue;
        }

        printf("\n%s: ", t->name);
        if (failed(t->status)) {
            nfail++;
            printf("failure");
        } else {
            nerr++;
            printf("error");
            if (WIFEXITED(t->status)) {
                printf(" (exit status %d)", WEXITSTATUS(t->status));
            }
            if (WIFSIGNALED(t->status)) {
                printf(" (signal %d)", WTERMSIG(t->status));
            }
        }

        putchar('\n');
        lseek(t->fd, 0, SEEK_SET);
        copyfd(stdout, t->fd);
    }

    if (nfail || nerr) {
        printf("\n%d failures; %d errors.\n", nfail, nerr);
    } else {
        printf("\nPASS\n");
    }
    return nfail || nerr;
}


int
readtokens()
{
    int n = 1;
    char c, *s;
    if ((s = strstr(getenv("MAKEFLAGS"), " --jobserver-fds="))) {
        rjobfd = (int)strtol(s+17, &s, 10);  // skip " --jobserver-fds="
        wjobfd = (int)strtol(s+1, NULL, 10); // skip comma
    }
    if (rjobfd >= 0) {
        fcntl(rjobfd, F_SETFL, fcntl(rjobfd, F_GETFL)|O_NONBLOCK);
        while (read(rjobfd, &c, 1) > 0) {
            n++;
        }
    }
    return n;
}


void
writetokens(int n)
{
    char c = '+';
    if (wjobfd >= 0) {
        fcntl(wjobfd, F_SETFL, fcntl(wjobfd, F_GETFL)|O_NONBLOCK);
        for (; n>1; n--) {
            write(wjobfd, &c, 1); // ignore error; nothing we can do anyway
        }
    }
}


int
main(int argc, char **argv)
{
    int n = readtokens();
    runalltest(ctmaintest, n);
    writetokens(n);
    int code = report(ctmaintest);
    if (code != 0) {
        return code;
    }
    if (argc == 2 && strcmp(argv[1], "-b") == 0) {
        runallbench(ctmainbench);
    }
    return 0;
}
