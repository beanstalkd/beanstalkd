/* Copyright (C) 2007 Keith Rarick and Philotic Inc.
 * Copyright 2011 Keith Rarick

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dat.h"


static void
set(Heap *h, int k, void *x)
{
    h->data[k] = x;
    h->rec(x, k);
}


static void
swap(Heap *h, int a, int b)
{
    void *tmp;

    tmp = h->data[a];
    set(h, a, h->data[b]);
    set(h, b, tmp);
}


static int
cmp(Heap *h, int a, int b)
{
    return h->cmp(h->data[a], h->data[b]);
}


static void
siftdown(Heap *h, int k)
{
    for (;;) {
        int p = (k-1) / 2; /* parent */

        if (k == 0 || cmp(h, k, p) >= 0) {
            return;
        }

        swap(h, k, p);
        k = p;
    }
}


static void
siftup(Heap *h, int k)
{
    for (;;) {
        int l, r, s;

        l = k*2 + 1; /* left child */
        r = k*2 + 2; /* right child */

        /* find the smallest of the three */
        s = k;
        if (l < h->len && cmp(h, l, s) < 0) s = l;
        if (r < h->len && cmp(h, r, s) < 0) s = r;

        if (s == k) {
            return; /* satisfies the heap property */
        }

        swap(h, k, s);
        k = s;
    }
}


int
heapinsert(Heap *h, void *x)
{
    int k;

    if (h->len == h->cap) {
        void **ndata;
        int ncap = (h->len+1) * 2; /* allocate twice what we need */

        ndata = malloc(sizeof(void*) * ncap);
        if (!ndata) {
            return 0;
        }

        memcpy(ndata, h->data, sizeof(void*)*h->len);
        free(h->data);
        h->data = ndata;
        h->cap = ncap;
    }

    k = h->len;
    h->len++;
    set(h, k, x);
    siftdown(h, k);
    return 1;
}


void *
heapremove(Heap *h, int k)
{
    void *x;

    if (k >= h->len) {
        return 0;
    }

    x = h->data[k];
    h->len--;
    set(h, k, h->data[h->len]);
    siftdown(h, k);
    siftup(h, k);
    h->rec(x, -1);
    return x;
}
