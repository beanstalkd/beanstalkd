#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

const char *progname;

static void
vwarnx(const char *err, const char *fmt, va_list args)
__attribute__((format(printf, 2, 0)));

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
#ifdef HAVE_LIBSYSTEMD
    if (sd_listen_fds(0) > 0) {
        warnx("inherited listen fd; ignoring option: %s %s", opt, arg);
    }
#endif
}


static void usage(int code) __attribute__ ((noreturn));
static void
usage(int code)
{
    fprintf(stderr, "Use: %s [OPTIONS]\n"
            "\n"
            "Options:\n"
            " -b DIR   write-ahead log directory\n"
            " -f MS    fsync at most once every MS milliseconds (default is %dms);\n"
            "          use -f0 for \"always fsync\"\n"
            " -F       never fsync\n"
            " -l ADDR  listen on address (default is 0.0.0.0)\n"
            " -p PORT  listen on port (default is " Portdef ")\n"
            " -u USER  become user and group\n"
            " -z BYTES set the maximum job size in bytes (default is %d);\n"
            "          max allowed is %d bytes\n"
            " -s BYTES set the size of each write-ahead log file (default is %d);\n"
            "          will be rounded up to a multiple of 4096 bytes\n"
            " -v       show version information\n"
            " -V       increase verbosity\n"
            " -h       show this help\n",
            progname,
            DEFAULT_FSYNC_MS,
            JOB_DATA_SIZE_LIMIT_DEFAULT,
            JOB_DATA_SIZE_LIMIT_MAX,
            Filesizedef);
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
    char *arg, *tmp;
#   define EARGF(x) (*arg ? (tmp=arg,arg="",tmp) : *argv ? *argv++ : (x))

    while ((arg = *argv++) && *arg++ == '-' && *arg) {
        char c;
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
                    if (job_data_size_limit > JOB_DATA_SIZE_LIMIT_MAX) {
                        warnx("maximum job size was set to %d", JOB_DATA_SIZE_LIMIT_MAX);
                        job_data_size_limit = JOB_DATA_SIZE_LIMIT_MAX;
                    }
                    break;
                case 's':
                    s->wal.filesize = parse_size_t(EARGF(flagusage("-s")));
                    break;
                case 'c':
                    warnx("-c flag was removed. binlog is always compacted.");
                    break;
                case 'n':
                    warnx("-n flag was removed. binlog is always compacted.");
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
