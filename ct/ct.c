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
#include "internal.h"
#include "ct.h"


static Test *curtest;
static int rjobfd = -1, wjobfd = -1;


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
    mkdir(curtest->dir, 0700);
    return curtest->dir;
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
failed(int s)
{
    return WIFSIGNALED(s) && (WTERMSIG(s) == SIGABRT);
}


static void
waittest(void)
{
    Test *t;
    int pid, stat;

    pid = wait3(&stat, 0, 0);
    if (pid == -1) {
        die(3, errno, "wait");
    }
    killpg(pid, 9);

    for (t=ctmain; t->f; t++) {
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
    FILE *out;
    out = tmpfile();
    if (!out) {
        die(1, errno, "tmpfile");
    }
    t->fd = fileno(out);
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
        curtest = t;
        t->f();
        _exit(0);
    }
    setpgid(t->pid, t->pid);
}


static void
runall(Test t[], int limit)
{
    int nrun = 0;
    for (; t->f; t++) {
        if (nrun >= limit) {
            waittest();
            nrun--;
        }
        start(t);
        nrun++;
    }
    for (; nrun; nrun--) {
        waittest();
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


static int
report(Test t[])
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
main()
{
    int n = readtokens();
    runall(ctmain, n);
    writetokens(n);
    return report(ctmain);
}
