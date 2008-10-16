
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "tube.h"
#include "job.h"
#include "util.h"
#include "pq.h"

static tube default_tube;

static void
elapsed(struct timeval *t0, struct timeval *t1)
{
    printf("elapsed %zdms\n",
            (1000 * (t1->tv_sec - t0->tv_sec)) +
            (t1->tv_usec / 1000) - (t0->tv_usec / 1000));
}

int
main(int argc, char **argv)
{
    int i;
    job j;
    struct timeval t0, t1, t2, t3, t4;

    progname = argv[0];

    job_init();
    TUBE_ASSIGN(default_tube, make_tube("default"));

    printf("inserting 2M ");
    fflush(stdout);
    gettimeofday(&t0, NULL);
    for (i = 0; i < 2000000; i++) {
        j = make_job(1, 0, 1, 0, default_tube);
    }
    gettimeofday(&t1, NULL);
    elapsed(&t0, &t1);

    printf("removing 200K ");
    fflush(stdout);
    for (i = 0; i < 200000; i++) {
        j = job_find(i);
        if (j) job_free(j);
    }
    gettimeofday(&t2, NULL);
    elapsed(&t1, &t2);

    printf("inserting 2M ");
    fflush(stdout);
    for (i = 0; i < 2000000; i++) {
        j = make_job(1, 0, 1, 0, default_tube);
    }
    gettimeofday(&t3, NULL);
    elapsed(&t2, &t3);

    printf("removing 200K ");
    fflush(stdout);
    for (i = 0; i < 200000; i++) {
        j = job_find(i + 2000000);
        if (j) job_free(j);
    }
    gettimeofday(&t4, NULL);
    elapsed(&t3, &t4);

    printf("total ");
    elapsed(&t0, &t4);
    return 0;
}
