#include "cut.h"
#include <stdlib.h>
#include <stdio.h>

#include "../tube.h"
#include "../job.h"
#include "../util.h"
#include "../pq.h"

static tube default_tube;

void
__CUT_BRINGUP__job()
{
    job_init();
    TUBE_ASSIGN(default_tube, make_tube("default"));
}

void
__CUT__job_test_creation()
{
    job j;

    j = make_job(1, 0, 1, 0, default_tube);
    ASSERT(j->pri == 1, "priority should match");
}

void
__CUT__job_test_cmp_pris()
{
    job a, b;

    a = make_job(1, 0, 1, 0, default_tube);
    b = make_job(1 << 27, 0, 1, 0, default_tube);

    ASSERT(job_pri_cmp(a, b) < 0, "should be a < b");
}

void
__CUT__job_test_cmp_ids()
{
    job a, b;

    a = make_job(1, 0, 1, 0, default_tube);
    b = make_job(1, 0, 1, 0, default_tube);

    b->id <<= 49;
    ASSERT(job_pri_cmp(a, b) < 0, "should be a < b");
}


void
__CUT__job_test_large_pris()
{
    job a, b;

    a = make_job(1, 0, 1, 0, default_tube);
    b = make_job(-5, 0, 1, 0, default_tube);

    ASSERT(job_pri_cmp(a, b) < 0, "should be a < b");

    a = make_job(-5, 0, 1, 0, default_tube);
    b = make_job(1, 0, 1, 0, default_tube);

    ASSERT(job_pri_cmp(a, b) > 0, "should be a > b");
}

void
__CUT_TAKEDOWN__job()
{
    TUBE_ASSIGN(default_tube, 0);
}

