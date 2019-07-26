#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "ct/ct.h"


void
cttest_heap_insert_one()
{
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };

    job j = make_job(1, 0, 1, 0, 0);
    assertf(j, "allocate job");

    heapinsert(&h, j);
    assertf(h.len == 1, "h should contain one item.");
    assertf(j->heap_index == 0, "should match");
}

void
cttest_heap_insert_and_remove_one()
{
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };

    job j1 = make_job(1, 0, 1, 0, 0);
    assertf(j1, "allocate job");

    int r = heapinsert(&h, j1);
    assertf(r, "insert should succeed");

    job got = heapremove(&h, 0);
    assertf(got == j1, "j1 should come back out");
    assertf(h.len == 0, "h should be empty.");
}

void
cttest_heap_priority()
{
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };
    job j, j1, j2, j3;

    j1 = make_job(1, 0, 1, 0, 0);
    j2 = make_job(2, 0, 1, 0, 0);
    j3 = make_job(3, 0, 1, 0, 0);
    assertf(j1, "allocate job");
    assertf(j2, "allocate job");
    assertf(j3, "allocate job");

    int r = heapinsert(&h, j2);
    assertf(r, "insert should succeed");
    assertf(j2->heap_index == 0, "should match");

    r = heapinsert(&h, j3);
    assertf(r, "insert should succeed");
    assertf(j2->heap_index == 0, "should match");
    assertf(j3->heap_index == 1, "should match");

    r = heapinsert(&h, j1);
    assertf(r, "insert should succeed");
    assertf(j1->heap_index == 0, "should match");
    assertf(j2->heap_index == 2, "should match");
    assertf(j3->heap_index == 1, "should match");

    j = heapremove(&h, 0);
    assertf(j == j1, "j1 should come out first.");
    assertf(j2->heap_index == 0, "should match");
    assertf(j3->heap_index == 1, "should match");

    j = heapremove(&h, 0);
    assertf(j == j2, "j2 should come out second.");
    assertf(j3->heap_index == 0, "should match");

    j = heapremove(&h, 0);
    assertf(j == j3, "j3 should come out third.");
}

void
cttest_heap_fifo_property()
{
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };
    job j, j3a, j3b, j3c;

    j3a = make_job(3, 0, 1, 0, 0);
    j3b = make_job(3, 0, 1, 0, 0);
    j3c = make_job(3, 0, 1, 0, 0);
    assertf(j3a, "allocate job");
    assertf(j3b, "allocate job");
    assertf(j3c, "allocate job");

    int r = heapinsert(&h, j3a);
    assertf(r, "insert should succeed");
    assertf(h.data[0] == j3a, "j3a should be in pos 0");
    assertf(j3a->heap_index == 0, "should match");

    r = heapinsert(&h, j3b);
    assertf(r, "insert should succeed");
    assertf(h.data[1] == j3b, "j3b should be in pos 1");
    assertf(j3a->heap_index == 0, "should match");
    assertf(j3b->heap_index == 1, "should match");

    r = heapinsert(&h, j3c);
    assertf(r, "insert should succeed");
    assertf(h.data[2] == j3c, "j3c should be in pos 2");
    assertf(j3a->heap_index == 0, "should match");
    assertf(j3b->heap_index == 1, "should match");
    assertf(j3c->heap_index == 2, "should match");

    j = heapremove(&h, 0);
    assertf(j == j3a, "j3a should come out first.");
    assertf(j3b->heap_index == 0, "should match");
    assertf(j3c->heap_index == 1, "should match");

    j = heapremove(&h, 0);
    assertf(j == j3b, "j3b should come out second.");
    assertf(j3c->heap_index == 0, "should match");

    j = heapremove(&h, 0);
    assertf(j == j3c, "j3c should come out third.");
}

void
cttest_heap_many_jobs()
{
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };
    const int n = 20;
    job j;

    int i;
    for (i = 0; i < n; i++) {
        j = make_job(1 + rand() % 8192, 0, 1, 0, 0);
        assertf(j, "allocation");
        int r = heapinsert(&h, j);
        assertf(r, "heapinsert");
    }

    uint last_pri = 0;
    for (i = 0; i < n; i++) {
        j = heapremove(&h, 0);
        assertf(j->r.pri >= last_pri, "should come out in order");
        last_pri = j->r.pri;
    }
}

void
cttest_heap_remove_k()
{
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };
    const int n = 20;

    int c, i;
    for (c = 0; c < 50; c++) {
        for (i = 0; i < n; i++) {
            job j = make_job(1 + rand() % 8192, 0, 1, 0, 0);
            assertf(j, "allocation");
            int r = heapinsert(&h, j);
            assertf(r, "heapinsert");
        }

        /* remove one from the middle */
        heapremove(&h, 25);

        /* now make sure the rest are still a valid heap */
        uint last_pri = 0;
        for (i = 1; i < n; i++) {
            job j = heapremove(&h, 0);
            assertf(j->r.pri >= last_pri, "should come out in order");
            last_pri = j->r.pri;
        }
    }
}

void
ctbench_heap_insert(int n)
{
    job *j = calloc(n, sizeof *j);
    int i;
    for (i = 0; i < n; i++) {
        j[i] = make_job(1, 0, 1, 0, 0);
        assert(j[i]);
        j[i]->r.pri = -j[i]->r.id;
    }
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };

    ctresettimer();
    for (i = 0; i < n; i++) {
        heapinsert(&h, j[i]);
    }
    ctstoptimer();

    for (i = 0; i < n; i++)
        job_free(heapremove(&h, 0));
    free(h.data);
    free(j);
}

void
ctbench_heap_remove(int n)
{
    Heap h = {
        .less = job_pri_less,
        .setpos = job_setpos,
    };
    int i;
    for (i = 0; i < n; i++) {
        job j = make_job(1, 0, 1, 0, 0);
        assertf(j, "allocate job");
        heapinsert(&h, j);
    }
    job t[n];   // temp storage to deallocate jobs later

    ctresettimer();
    for (i = 0; i < n; i++) {
        t[i] = heapremove(&h, 0);
    }
    ctstoptimer();

    free(h.data);
    for (i = 0; i < n; i++)
        job_free(t[i]);
}
