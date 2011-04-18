#include "../config.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "../cut.h"
#include "../dat.h"


void
__CUT_BRINGUP__heap()
{
}


void
__CUT__heap_test_insert_one()
{
    Heap h = {0};
    job j;

    h.cmp = job_pri_cmp;
    h.rec = job_setheappos;

    j = make_job(1, 0, 1, 0, 0);
    ASSERT(j, "allocate job");

    heapinsert(&h, j);
    ASSERT(h.len == 1, "h should contain one item.");
    ASSERT(j->heap_index == 0, "should match");
}


void
__CUT__heap_test_insert_and_remove_one()
{
    Heap h = {0};
    int r;
    job j, j1;

    h.cmp = job_pri_cmp;
    h.rec = job_setheappos;
    j1 = make_job(1, 0, 1, 0, 0);
    ASSERT(j1, "allocate job");

    r = heapinsert(&h, j1);
    ASSERT(r, "insert should succeed");

    j = heapremove(&h, 0);
    ASSERT(j == j1, "j1 should come back out");
    ASSERT(h.len == 0, "h should be empty.");
    printf("j->heap_index is %zu\n", j->heap_index);
    ASSERT(j->heap_index == -1, "j's heap index should be invalid");
}


void
__CUT__heap_test_priority()
{
    Heap h = {0};
    int r;
    job j, j1, j2, j3;

    h.cmp = job_pri_cmp;
    h.rec = job_setheappos;
    j1 = make_job(1, 0, 1, 0, 0);
    j2 = make_job(2, 0, 1, 0, 0);
    j3 = make_job(3, 0, 1, 0, 0);
    ASSERT(j1, "allocate job");
    ASSERT(j2, "allocate job");
    ASSERT(j3, "allocate job");

    r = heapinsert(&h, j2);
    ASSERT(r, "insert should succeed");
    ASSERT(j2->heap_index == 0, "should match");

    r = heapinsert(&h, j3);
    ASSERT(r, "insert should succeed");
    ASSERT(j2->heap_index == 0, "should match");
    ASSERT(j3->heap_index == 1, "should match");

    r = heapinsert(&h, j1);
    ASSERT(r, "insert should succeed");
    ASSERT(j1->heap_index == 0, "should match");
    ASSERT(j2->heap_index == 2, "should match");
    ASSERT(j3->heap_index == 1, "should match");

    j = heapremove(&h, 0);
    ASSERT(j == j1, "j1 should come out first.");
    ASSERT(j2->heap_index == 0, "should match");
    ASSERT(j3->heap_index == 1, "should match");

    j = heapremove(&h, 0);
    ASSERT(j == j2, "j2 should come out second.");
    ASSERT(j3->heap_index == 0, "should match");

    j = heapremove(&h, 0);
    ASSERT(j == j3, "j3 should come out third.");
}


void
__CUT__heap_test_fifo_property()
{
    Heap h = {0};
    int r;
    job j, j3a, j3b, j3c;

    h.cmp = job_pri_cmp;
    h.rec = job_setheappos;
    j3a = make_job(3, 0, 1, 0, 0);
    j3b = make_job(3, 0, 1, 0, 0);
    j3c = make_job(3, 0, 1, 0, 0);
    ASSERT(j3a, "allocate job");
    ASSERT(j3b, "allocate job");
    ASSERT(j3c, "allocate job");

    r = heapinsert(&h, j3a);
    ASSERT(r, "insert should succeed");
    ASSERT(h.data[0] == j3a, "j3a should be in pos 0");
    ASSERT(j3a->heap_index == 0, "should match");

    r = heapinsert(&h, j3b);
    ASSERT(r, "insert should succeed");
    ASSERT(h.data[1] == j3b, "j3b should be in pos 1");
    ASSERT(j3a->heap_index == 0, "should match");
    ASSERT(j3b->heap_index == 1, "should match");

    r = heapinsert(&h, j3c);
    ASSERT(r, "insert should succeed");
    ASSERT(h.data[2] == j3c, "j3c should be in pos 2");
    ASSERT(j3a->heap_index == 0, "should match");
    ASSERT(j3b->heap_index == 1, "should match");
    ASSERT(j3c->heap_index == 2, "should match");

    j = heapremove(&h, 0);
    ASSERT(j == j3a, "j3a should come out first.");
    ASSERT(j3b->heap_index == 0, "should match");
    ASSERT(j3c->heap_index == 1, "should match");

    j = heapremove(&h, 0);
    ASSERT(j == j3b, "j3b should come out second.");
    ASSERT(j3c->heap_index == 0, "should match");

    j = heapremove(&h, 0);
    ASSERT(j == j3c, "j3c should come out third.");
}


void
__CUT__heap_test_many_jobs()
{
    Heap h = {0};
    uint last_pri;
    int r, i, n = 20;
    job j;

    h.cmp = job_pri_cmp;
    h.rec = job_setheappos;

    for (i = 0; i < n; i++) {
        j = make_job(1 + rand() % 8192, 0, 1, 0, 0);
        ASSERT(j, "allocation");
        r = heapinsert(&h, j);
        ASSERT(r, "heapinsert");
    }

    last_pri = 0;
    for (i = 0; i < n; i++) {
        j = heapremove(&h, 0);
        ASSERT(j->pri >= last_pri, "should come out in order");
        last_pri = j->pri;
    }
}


void
__CUT__heap_test_remove_k()
{
    Heap h = {0};
    uint last_pri;
    int r, i, c, n = 20;
    job j;

    h.cmp = job_pri_cmp;
    h.rec = job_setheappos;

    for (c = 0; c < 50; c++) {
        for (i = 0; i < n; i++) {
            j = make_job(1 + rand() % 8192, 0, 1, 0, 0);
            ASSERT(j, "allocation");
            r = heapinsert(&h, j);
            ASSERT(r, "heapinsert");
        }

        /* remove one from the middle */
        heapremove(&h, 25);

        /* now make sure the rest are still a valid heap */
        last_pri = 0;
        for (i = 1; i < n; i++) {
            j = heapremove(&h, 0);
            ASSERT(j->pri >= last_pri, "should come out in order");
            last_pri = j->pri;
        }
    }
}

void
__CUT_TAKEDOWN__heap()
{
}
