/* CT - (Relatively) Easy Unit Testing for C */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "internal.h"

static void die(int code, int err, const char *msg);
static int  failed(int s);


void
ctrun(T *t, int i, void (*f)(void), const char *name)
{
    pid_t pid;
    int r;
    FILE *out;

    if (i % 10 == 0) {
        if (i % 50 == 0) {
            putchar('\n');
        }
        printf("%5d", i);
    }

    t->name = name;

    out = tmpfile();
    if (!out) die(1, errno, "tmpfile");
    t->fd = fileno(out);

    fflush(stdout);
    fflush(stderr);

    pid = fork();
    if (pid < 0) {
        die(1, errno, "fork");
    } else if (!pid) {
        r = dup2(t->fd, 1); // send stdout to tmpfile
        if (r == -1) die(3, errno, "dup2");

        r = close(t->fd);
        if (r == -1) die(3, errno, "fclose");

        r = dup2(1, 2); // send stderr to stdout
        if (r < 0) die(3, errno, "dup2");

        f();
        exit(0);
    }

    r = waitpid(pid, &t->status, 0);
    if (r != pid) die(3, errno, "wait");

    if (!t->status) {
        putchar('.');
    } else if (failed(t->status)) {
        putchar('F');
    } else {
        putchar('E');
    }

    fflush(stdout);
}


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


static int
failed(int s)
{
    return WIFSIGNALED(s) && (WTERMSIG(s) == SIGABRT);
}


void
ctreport(T ts[], int n)
{
    int i, r, s;
    char buf[1024]; // arbitrary size
    int cf = 0, ce = 0;

    putchar('\n');
    for (i = 0; i < n; i++) {
        if (!ts[i].status) continue;

        printf("\n%s: ", ts[i].name);
        if (failed(ts[i].status)) {
            cf++;
            printf("failure");
        } else {
            ce++;
            printf("error");
            if (WIFEXITED(ts[i].status)) {
                printf(" (exit status %d)", WEXITSTATUS(ts[i].status));
            }
            if (WIFSIGNALED(ts[i].status)) {
                printf(" (signal %d)", WTERMSIG(ts[i].status));
            }
        }

        putchar('\n');
        lseek(ts[i].fd, 0, SEEK_SET);
        while ((r = read(ts[i].fd, buf, sizeof(buf)))) {
            s = fwrite(buf, 1, r, stdout);
            if (r != s) die(3, errno, "fwrite");
        }
    }

    printf("\n%d tests; %d failures; %d errors.\n", n, cf, ce);
    exit(cf || ce);
}


static void
die(int code, int err, const char *msg)
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
