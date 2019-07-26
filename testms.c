#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "ct/ct.h"


void
cttest_ms_append()
{
    Ms *a = new(struct Ms);
    ms_init(a, NULL, NULL);

    int i = 10;

    int ok = ms_append(a, &i);
    assertf(a->len == 1, "a should contain one item");
    assertf(ok, "should be added");

    ok = ms_append(a, &i);
    assertf(a->len == 2, "a should contain two items");
    assertf(ok, "should be added");

    ms_clear(a);
    assertf(a->len == 0, "a should be empty");
    free(a->items);
    free(a);
}

void
cttest_ms_remove()
{

    Ms *a = new(struct Ms);
    ms_init(a, NULL, NULL);

    int i = 1;
    ms_append(a, &i);

    int j = 2;
    int ok = ms_remove(a, &j);
    assertf(!ok, "j should not be removed");

    ok = ms_remove(a, &i);
    assertf(ok, "i should be removed");

    ok = ms_remove(a, &i);
    assertf(!ok, "i was already removed");

    assertf(a->len == 0, "a should be empty");
    free(a->items);
    free(a);
}

void
cttest_ms_contains()
{

    Ms *a = new(struct Ms);
    ms_init(a, NULL, NULL);

    int i = 1;
    ms_append(a, &i);
    int ok = ms_contains(a, &i);
    assertf(ok, "i should be in a");

    int j = 2;
    ok = ms_contains(a, &j);
    assertf(!ok, "j should not be in a");

    ms_clear(a);
    free(a->items);
    free(a);
}

void
cttest_ms_clear_empty()
{

    Ms *a = new(struct Ms);
    ms_init(a, NULL, NULL);

    ms_clear(a);
    assertf(a->len == 0, "a should be empty");
    free(a->items);
    free(a);
}

void
cttest_ms_take()
{
    Ms *a = new(struct Ms);
    ms_init(a, NULL, NULL);

    int i = 10;
    int j = 20;

    ms_append(a, &i);
    ms_append(a, &j);

    int *n;
    n = (int *)ms_take(a);
    assertf(n == &i, "n should point to i");

    n = (int *)ms_take(a);
    assertf(n == &j, "n should point to j");

    n = (int *)ms_take(a);
    assertf(n == NULL, "n should be NULL; ms is empty");

    free(a->items);
    free(a);
}

void
cttest_ms_take_sequence()
{
    size_t i;
    int s[] = {1, 2, 3, 4, 5, 6};
    int e[] = {1, 2, 3, 6, 5, 4};

    Ms *a = new(struct Ms);
    ms_init(a, NULL, NULL);

    size_t n = sizeof(s)/sizeof(s[0]);
    for (i = 0; i < n; i++)
        ms_append(a, &s[i]);

    for (i = 0; i < n; i++) {
        int *got = (int *)ms_take(a);
        assert(*got == e[i]);
    }

    free(a->items);
    free(a);
}

