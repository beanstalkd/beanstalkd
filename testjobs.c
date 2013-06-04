#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "ct/ct.h"
#include "dat.h"

static tube default_tube;

void
cttestjob_creation()
{
    job j;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    j = make_job(1, 0, 1, 0, default_tube);
    assertf(j->r.pri == 1, "priority should match");
}

void
cttestjob_cmp_pris()
{
    job a, b;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    a = make_job(1, 0, 1, 0, default_tube);
    b = make_job(1 << 27, 0, 1, 0, default_tube);

    assertf(job_pri_less(a, b), "should be less");
}

void
cttestjob_cmp_ids()
{
    job a, b;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    a = make_job(1, 0, 1, 0, default_tube);
    b = make_job(1, 0, 1, 0, default_tube);

    b->r.id <<= 49;
    assertf(job_pri_less(a, b), "should be less");
}


void
cttestjob_large_pris()
{
    job a, b;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    a = make_job(1, 0, 1, 0, default_tube);
    b = make_job(-5, 0, 1, 0, default_tube);

    assertf(job_pri_less(a, b), "should be less");

    a = make_job(-5, 0, 1, 0, default_tube);
    b = make_job(1, 0, 1, 0, default_tube);

    assertf(!job_pri_less(a, b), "should not be less");
}

void
cttestjob_hash_free()
{
    job j;
    uint64 jid = 83;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    j = make_job_with_id(0, 0, 1, 0, default_tube, jid);
    job_free(j);

    assertf(!job_find(jid), "job should be missing");
}

void
cttestjob_hash_free_next()
{
    job a, b;
    uint64 aid = 97, bid = 12386;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    b = make_job_with_id(0, 0, 1, 0, default_tube, bid);
    a = make_job_with_id(0, 0, 1, 0, default_tube, aid);

    assertf(a->ht_next == b, "b should be chained to a");

    job_free(b);

    assertf(a->ht_next == NULL, "job should be missing");
}

void
cttestjob_all_jobs_used()
{
    job j, x;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    j = make_job(0, 0, 1, 0, default_tube);
    assertf(get_all_jobs_used() == 1, "should match");

    x = allocate_job(10);
    assertf(get_all_jobs_used() == 1, "should match");

    job_free(x);
    assertf(get_all_jobs_used() == 1, "should match");

    job_free(j);
    assertf(get_all_jobs_used() == 0, "should match");
}

void
cttestjob_100_000_jobs()
{
    int i;

    TUBE_ASSIGN(default_tube, make_tube("default"));
    for (i = 0; i < 100000; i++) {
        make_job(0, 0, 1, 0, default_tube);
    }
    assertf(get_all_jobs_used() == 100000, "should match");

    for (i = 1; i <= 100000; i++) {
        job_free(job_find(i));
    }
    fprintf(stderr, "get_all_jobs_used() => %zu\n", get_all_jobs_used());
    assertf(get_all_jobs_used() == 0, "should match");
}

void
ctbenchmakejob(int n)
{
    int i;
    TUBE_ASSIGN(default_tube, make_tube("default"));
    for (i = 0; i < n; i++) {
        make_job(0, 0, 1, 0, default_tube);
    }
}
