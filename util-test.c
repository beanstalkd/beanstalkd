#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "ct/ct.h"
#include "dat.h"

void
cttestallocf()
{
    char *got;

    got = fmtalloc("hello, %s %d", "world", 5);
    assertf(strcmp("hello, world 5", got) == 0, "got \"%s\"", got);
}


void
cttestoptnone()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        NULL,
    };

    optparse(&s, args);
    assert(strcmp(s.port, Portdef) == 0);
    assert(s.addr == NULL);
    assert(job_data_size_limit == JOB_DATA_SIZE_LIMIT_DEFAULT);
    assert(s.wal.filesz == Filesizedef);
    assert(s.wal.nocomp == 0);
    assert(s.wal.wantsync == 0);
    assert(s.user == NULL);
    assert(s.wal.dir == NULL);
    assert(s.wal.use == 0);
    assert(verbose == 0);
}


void
cttestoptp()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-p1234",
        NULL,
    };

    optparse(&s, args);
    assert(strcmp(s.port, "1234") == 0);
}


void
cttestoptl()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-llocalhost",
        NULL,
    };

    optparse(&s, args);
    assert(strcmp(s.addr, "localhost") == 0);
}


void
cttestoptz()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-z1234",
        NULL,
    };

    optparse(&s, args);
    assert(job_data_size_limit == 1234);
}


void
cttestopts()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-s1234",
        NULL,
    };

    optparse(&s, args);
    assert(s.wal.filesz == 1234);
}


void
cttestoptc()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-n",
        "-c",
        NULL,
    };

    optparse(&s, args);
    assert(s.wal.nocomp == 0);
}


void
cttestoptn()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-n",
        NULL,
    };

    optparse(&s, args);
    assert(s.wal.nocomp == 1);
}


void
cttestoptf()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-f1234",
        NULL,
    };

    optparse(&s, args);
    assert(s.wal.syncrate == 1234000000);
    assert(s.wal.wantsync == 1);
}


void
cttestoptF()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-f1234",
        "-F",
        NULL,
    };

    optparse(&s, args);
    assert(s.wal.wantsync == 0);
}


void
cttestoptu()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-ukr",
        NULL,
    };

    optparse(&s, args);
    assert(strcmp(s.user, "kr") == 0);
}


void
cttestoptb()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-bfoo",
        NULL,
    };

    optparse(&s, args);
    assert(strcmp(s.wal.dir, "foo") == 0);
    assert(s.wal.use == 1);
}


void
cttestoptV()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-V",
        NULL,
    };

    optparse(&s, args);
    assert(verbose == 1);
}


void
cttestoptV_V()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-V",
        "-V",
        NULL,
    };

    optparse(&s, args);
    assert(verbose == 2);
}


void
cttestoptVVV()
{
    Srv s = {};
    s.port = Portdef;
    s.wal.filesz = Filesizedef;
    char *args[] = {
        "-VVV",
        NULL,
    };

    optparse(&s, args);
    assert(verbose == 3);
}
