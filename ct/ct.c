// CT - simple-minded unit testing for C

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "internal.h"


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
run(T t[])
{
    int pid;
    FILE *out;

    for (; t->f; t++) {
        out = tmpfile();
        if (!out) {
            die(1, errno, "tmpfile");
        }
        t->fd = fileno(out);
        pid = fork();
        if (pid < 0) {
            die(1, errno, "fork");
        } else if (!pid) {
            if (dup2(t->fd, 1) == -1) {
                die(3, errno, "dup2");
            }
            if (close(t->fd) == -1) {
                die(3, errno, "fclose");
            }
            if (dup2(1, 2) == -1) {
                die(3, errno, "dup2");
            }
            t->f();
            exit(0);
        }

        if (waitpid(pid, &t->status, 0) != pid) {
            die(3, errno, "wait");
        }

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


static void
copyfd(int out, int in)
{
    int n;
    char buf[1024]; // arbitrary size

    while ((n = read(in, buf, sizeof(buf))) != 0) {
        if (write(out, buf, n) != n) {
            die(3, errno, "write");
        }
    }
}


static int
report(T t[])
{
    int nfail = 0, nerr = 0;

    putchar('\n');
    for (; t->f; t++) {
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
        copyfd(1, t->fd);
    }

    if (nfail || nerr) {
        printf("\n%d failures; %d errors.\n", nfail, nerr);
    } else {
        printf("\nPASS\n");
    }
    return nfail || nerr;
}


int
main(int argc, char *argv[])
{
    run(ctmain);
    return report(ctmain);
}
