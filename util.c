#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "sd-daemon.h"
#include "dat.h"

const char *progname;

void
v()
{
}

static void
vwarnx(const char *err, const char *fmt, va_list args)
{
    fprintf(stderr, "%s: ", progname);
    if (fmt) {
        vfprintf(stderr, fmt, args);
        if (err) fprintf(stderr, ": %s", err);
    }
    fputc('\n', stderr);
}

void
warn(const char *fmt, ...)
{
    char *err = strerror(errno); /* must be done first thing */
    va_list args;

    va_start(args, fmt);
    vwarnx(err, fmt, args);
    va_end(args);
}

void
warnx(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vwarnx(NULL, fmt, args);
    va_end(args);
}


char*
fmtalloc(char *fmt, ...)
{
    int n;
    char *buf;
    va_list ap;

    // find out how much space is needed
    va_start(ap, fmt);
    n = vsnprintf(0, 0, fmt, ap) + 1; // include space for trailing NUL
    va_end(ap);

    buf = malloc(n);
    if (buf) {
        va_start(ap, fmt);
        vsnprintf(buf, n, fmt, ap);
        va_end(ap);
    }
    return buf;
}


// Zalloc allocates n bytes of zeroed memory and
// returns a pointer to it.
// If insufficient memory is available, zalloc returns 0.
void*
zalloc(int n)
{
    void *p;

    p = malloc(n);
    if (p) {
        memset(p, 0, n);
    }
    return p;
}


static void
warn_systemd_ignored_option(char *opt, char *arg)
{
    if (sd_listen_fds(0) > 0) {
        warnx("inherited listen fd; ignoring option: %s %s", opt, arg);
    }
}


static void usage(int code) __attribute__ ((noreturn));
static void
usage(int code)
{
    fprintf(stderr, "Use: %s [OPTIONS]\n"
            "\n"
            "Options:\n"
            " -b DIR   wal directory\n"
            " -f MS    fsync at most once every MS milliseconds"
                       " (use -f0 for \"always fsync\")\n"
            " -F       never fsync (default)\n"
            " -l ADDR  listen on address (default is 0.0.0.0)\n"
            " -p PORT  listen on port (default is " Portdef ")\n"
            " -u USER  become user and group\n"
            " -z BYTES set the maximum job size in bytes (default is %d)\n"
            " -s BYTES set the size of each wal file (default is %d)\n"
            "            (will be rounded up to a multiple of 512 bytes)\n"
            " -c       compact the binlog (default)\n"
            " -n       do not compact the binlog\n"
            " -v       show version information\n"
            " -V       increase verbosity\n"
            " -h       show this help\n",
            progname, JOB_DATA_SIZE_LIMIT_DEFAULT, Filesizedef);
    exit(code);
}


static char *flagusage(char *flag) __attribute__ ((noreturn));
static char *
flagusage(char *flag)
{
    warnx("flag requires an argument: %s", flag);
    usage(5);
}


static size_t
parse_size_t(char *str)
{
    char r, x;
    size_t size;

    r = sscanf(str, "%zu%c", &size, &x);
    if (1 != r) {
        warnx("invalid size: %s", str);
        usage(5);
    }
    return size;
}


void
optparse(Server *s, char **argv)
{
    int64 ms;
    char *arg, c, *tmp;
#   define EARGF(x) (*arg ? (tmp=arg,arg="",tmp) : *argv ? *argv++ : (x))

    while ((arg = *argv++) && *arg++ == '-') {
        while ((c = *arg++)) {
            switch (c) {
                case 'p':
                    s->port = EARGF(flagusage("-p"));
                    warn_systemd_ignored_option("-p", s->port);
                    break;
                case 'l':
                    s->addr = EARGF(flagusage("-l"));
                    warn_systemd_ignored_option("-l", s->addr);
                    break;
                case 'z':
                    job_data_size_limit = parse_size_t(EARGF(flagusage("-z")));
                    break;
                case 's':
                    s->wal.filesz = parse_size_t(EARGF(flagusage("-s")));
                    break;
                case 'c':
                    s->wal.nocomp = 0;
                    break;
                case 'n':
                    s->wal.nocomp = 1;
                    break;
                case 'f':
                    ms = (int64)parse_size_t(EARGF(flagusage("-f")));
                    s->wal.syncrate = ms * 1000000;
                    s->wal.wantsync = 1;
                    break;
                case 'F':
                    s->wal.wantsync = 0;
                    break;
                case 'u':
                    s->user = EARGF(flagusage("-u"));
                    break;
                case 'b':
                    s->wal.dir = EARGF(flagusage("-b"));
                    s->wal.use = 1;
                    break;
                case 'h':
                    usage(0);
                case 'v':
                    printf("beanstalkd %s\n", version);
                    exit(0);
                case 'V':
                    verbose++;
                    break;
                default:
                    warnx("unknown flag: %s", arg-2);
                    usage(5);
            }
        }
    }
    if (arg) {
        warnx("unknown argument: %s", arg-1);
        usage(5);
    }
}
