#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static void
set(Heap *h, size_t k, void *x)
{
    h->data[k] = x;
    h->setpos(x, k);
}


static void
swap(Heap *h, size_t a, size_t b)
{
    void *tmp;

    tmp = h->data[a];
    set(h, a, h->data[b]);
    set(h, b, tmp);
}


static int
less(Heap *h, size_t a, size_t b)
{
    return h->less(h->data[a], h->data[b]);
}


static void
siftdown(Heap *h, size_t k)
{
    for (;;) {
        size_t p = (k-1) / 2; /* parent */

        if (k == 0 || less(h, p, k)) {
            return;
        }

        swap(h, k, p);
        k = p;
    }
}


static void
siftup(Heap *h, size_t k)
{
    for (;;) {
        size_t l = k*2 + 1; /* left child */
        size_t r = k*2 + 2; /* right child */

        /* find the smallest of the three */
        size_t s = k;
        if (l < h->len && less(h, l, s)) s = l;
        if (r < h->len && less(h, r, s)) s = r;

        if (s == k) {
            return; /* satisfies the heap property */
        }

        swap(h, k, s);
        k = s;
    }
}


// Heapinsert inserts x into heap h according to h->less.
// It returns 1 on success, otherwise 0.
int
heapinsert(Heap *h, void *x)
{
    if (h->len == h->cap) {
        void **ndata;
        size_t ncap = (h->len+1) * 2; /* allocate twice what we need */

        ndata = malloc(sizeof(void*) * ncap);
        if (!ndata) {
            return 0;
        }

        memcpy(ndata, h->data, sizeof(void*) * h->len);
        free(h->data);
        h->data = ndata;
        h->cap = ncap;
    }

    size_t k = h->len;
    h->len++;
    set(h, k, x);
    siftdown(h, k);
    return 1;
}


void *
heapremove(Heap *h, size_t k)
{
    if (k >= h->len) {
        return 0;
    }

    void *x = h->data[k];
    h->len--;
    set(h, k, h->data[h->len]);
    siftdown(h, k);
    siftup(h, k);
    return x;
}
