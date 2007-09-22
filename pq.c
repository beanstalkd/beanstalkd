/* pq.c - priority queue */

#include <stdlib.h>

#include "pq.h"

pq
make_pq()
{
    pq q;

    q = malloc(sizeof(struct pq));
    if (!q) return NULL;

    q->size = HEAP_SIZE;
    q->heap = malloc(HEAP_SIZE * sizeof(job));
    return q;
}

static void
swap(pq q, unsigned int a, unsigned int b)
{
    job j;

    j = q->heap[a];
    q->heap[a] = q->heap[b];
    q->heap[b] = j;
}

#define PARENT(i) ((i)>>1)
#define CHILD_LEFT(i) (((i)<<1)+1)
#define CHILD_RIGHT(i) (((i)<<1)+2)

static void
bubble_up(pq q, unsigned int k)
{
    int p;

    if (k == 0) return;
    p = PARENT(k);
    if (q->heap[p]->pri < q->heap[k]->pri) return;
    swap(q, k, p);
    bubble_up(q, p);
}

static void
bubble_down(pq q, unsigned int k)
{
    int l, r, s;

    l = CHILD_LEFT(k);
    r = CHILD_RIGHT(k);

    s = k;
    if (l < q->used && q->heap[l]->pri < q->heap[k]->pri) s = l;
    if (r < q->used && q->heap[r]->pri < q->heap[s]->pri) s = r;
    if (s == k) return; /* already satisfies the heap property */

    swap(q, k, s);
    bubble_down(q, s);
}

static void
delete_min(pq q)
{
    if (!q->heap[0])

    q->heap[0] = q->heap[--q->used];
    if (q->used) bubble_down(q, 0);
}

int
pq_give(pq q, job j)
{
    int k;

    if (q->used >= q->size) return 0;

    k = q->used++;
    q->heap[k] = j;
    bubble_up(q, k);

    return 1;
}

job
pq_take(pq q)
{
    job j;

    if (q->used == 0) return NULL;

    j = q->heap[0];
    delete_min(q);
    return j;
}
