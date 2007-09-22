#include "cut.h"
#include <stdlib.h>
#include <stdio.h>

#include "../pq.h"

void
__CUT_BRINGUP__job()
{
}

void
__CUT__job_test_creation()
{
    job j;

    j = make_job(1, 0);
    ASSERT(j->pri == 1, "priority should match");
}

void
__CUT__job_test_cmp_pris()
{
    job a, b;

    a = make_job(1, 0);
    b = make_job(1 << 27, 0);

    ASSERT(job_cmp(a, b) < 0, "should be a < b");
}

void
__CUT__job_test_cmp_ids()
{
    job a, b;

    a = make_job(1, 0);
    b = make_job(1, 0);

    b->id <<= 49;
    ASSERT(job_cmp(a, b) < 0, "should be a < b");
}

void
__CUT_TAKEDOWN__job()
{
}

