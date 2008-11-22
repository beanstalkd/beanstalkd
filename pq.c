/* pq.c - priority queue */

/* Copyright (C) 2007 Keith Rarick and Philotic Inc.

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tube.h" /* hack to make cpp happy */
#include "pq.h"

void
pq_init(pq q, job_cmp_fn cmp)
{
    if (!q) return;

    q->cap = 0;
    q->used = 0;
    q->cmp = cmp;
    q->heap = NULL;

    return;
}

void
pq_clear(pq q)
{
    free(q->heap);
    pq_init(q, q->cmp);
}

static void
pq_grow(pq q)
{
    job *nheap;
    unsigned int ncap = q->cap << 1 ? : 1;

    nheap = malloc(ncap * sizeof(job));
    if (!nheap) return;

    if (q->heap) memcpy(nheap, q->heap, q->used * sizeof(job));
    free(q->heap);
    q->heap = nheap;
    q->cap = ncap;
}

static void
swap(pq q, unsigned int a, unsigned int b)
{
    job j;

    j = q->heap[a];
    q->heap[a] = q->heap[b];
    q->heap[b] = j;

    q->heap[a]->heap_index = a;
    q->heap[b]->heap_index = b;
}

#define PARENT(i) (((i-1))>>1)
#define CHILD_LEFT(i) (((i)<<1)+1)
#define CHILD_RIGHT(i) (((i)<<1)+2)

static int
cmp(pq q, unsigned int a, unsigned int b)
{
    return q->cmp(q->heap[a], q->heap[b]);
}

static void
bubble_up(pq q, unsigned int k)
{
    int p;

    if (k == 0) return;
    p = PARENT(k);
    if (cmp(q, p, k) <= 0) return;
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
    if (l < q->used && cmp(q, l, k) < 0) s = l;
    if (r < q->used && cmp(q, r, s) < 0) s = r;
    if (s == k) return; /* already satisfies the heap property */

    swap(q, k, s);
    bubble_down(q, s);
}

/* assumes there is at least one item in the queue */
static void
delete_min(pq q)
{
    q->heap[0] = q->heap[--q->used];
    q->heap[0]->heap_index = 0;
    if (q->used) bubble_down(q, 0);
}

int
pq_give(pq q, job j)
{
    int k;

    if (q->used >= q->cap) pq_grow(q);
    if (q->used >= q->cap) return 0;

    k = q->used++;
    q->heap[k] = j;
    j->heap_index = k;
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

job
pq_peek(pq q)
{
    if (q->used == 0) return NULL;
    return q->heap[0];
}

job
pq_remove(pq q, job j)
{
    unsigned long long int id;
    unsigned int pri;

    if (q->used == 0) return NULL;
    if (q->heap[j->heap_index] != j) return NULL;

    id = j->id;
    j->id = 0;
    pri = j->pri;
    j->pri = 0;

    bubble_up(q, j->heap_index);

    j->id = id;
    j->pri = pri;

    /* can't happen */
    if (q->heap[0] != j) return NULL;

    delete_min(q);

    return j;
}
