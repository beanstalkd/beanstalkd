/* ms.c - resizable multiset implementation */

/* Copyright (C) 2008 Keith Rarick and Philotic Inc.

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

#include "t.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <event.h>

#include "dat.h"

void
ms_init(ms a, ms_event_fn oninsert, ms_event_fn onremove)
{
    a->used = a->cap = a->last = 0;
    a->items = NULL;
    a->oninsert = oninsert;
    a->onremove = onremove;
}

static void
grow(ms a)
{
    void **nitems;
    size_t ncap = (a->cap << 1) ? : 1;

    nitems = malloc(ncap * sizeof(void *));
    if (!nitems) return;

    memcpy(nitems, a->items, a->used * sizeof(void *));
    free(a->items);
    a->items = nitems;
    a->cap = ncap;
}

int
ms_append(ms a, void *item)
{
    if (a->used >= a->cap) grow(a);
    if (a->used >= a->cap) return 0;

    a->items[a->used++] = item;
    if (a->oninsert) a->oninsert(a, item, a->used - 1);
    return 1;
}

static int
ms_delete(ms a, size_t i)
{
    void *item;

    if (i >= a->used) return 0;
    item = a->items[i];
    a->items[i] = a->items[--a->used];

    /* it has already been removed now */
    if (a->onremove) a->onremove(a, item, i);
    return 1;
}

void
ms_clear(ms a)
{
    while (ms_delete(a, 0));
    free(a->items);
    ms_init(a, a->oninsert, a->onremove);
}

int
ms_remove(ms a, void *item)
{
    size_t i;

    for (i = 0; i < a->used; i++) {
        if (a->items[i] == item) return ms_delete(a, i);
    }
    return 0;
}

int
ms_contains(ms a, void *item)
{
    size_t i;

    for (i = 0; i < a->used; i++) {
        if (a->items[i] == item) return 1;
    }
    return 0;
}

void *
ms_take(ms a)
{
    void *item;

    if (!a->used) return NULL;

    a->last = a->last % a->used;
    item = a->items[a->last];
    ms_delete(a, a->last);
    ++a->last;
    return item;
}
