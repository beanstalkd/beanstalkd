/* tube.c - tubes implementation */

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

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <event.h>

#include "dat.h"

struct ms tubes;

tube
make_tube(const char *name)
{
    tube t;

    t = malloc(sizeof(struct tube));
    if (!t) return NULL;

    t->refs = 0;

    t->name[MAX_TUBE_NAME_LEN - 1] = '\0';
    strncpy(t->name, name, MAX_TUBE_NAME_LEN - 1);
    if (t->name[MAX_TUBE_NAME_LEN - 1] != '\0') twarnx("truncating tube name");

    t->ready.cmp = job_pri_cmp;
    t->delay.cmp = job_delay_cmp;
    t->ready.rec = job_setheappos;
    t->delay.rec = job_setheappos;
    t->buried = (struct job) { };
    t->buried.prev = t->buried.next = &t->buried;
    ms_init(&t->waiting, NULL, NULL);

    t->stat = (struct stats) {0, 0, 0, 0, 0};
    t->using_ct = t->watching_ct = 0;
    t->deadline_at = t->pause = 0;

    return t;
}

static void
tube_free(tube t)
{
    prot_remove_tube(t);
    free(t->ready.data);
    free(t->delay.data);
    ms_clear(&t->waiting);
    free(t);
}

void
tube_dref(tube t)
{
    if (!t) return;
    if (t->refs < 1) return twarnx("refs is zero for tube: %s", t->name);

    --t->refs;
    if (t->refs < 1) tube_free(t);
}

void
tube_iref(tube t)
{
    if (!t) return;
    ++t->refs;
}

static tube
make_and_insert_tube(const char *name)
{
    int r;
    tube t = NULL;

    t = make_tube(name);
    if (!t) return NULL;

    /* We want this global tube list to behave like "weak" refs, so don't
     * increment the ref count. */
    r = ms_append(&tubes, t);
    if (!r) return tube_dref(t), (tube) 0;

    return t;
}

tube
tube_find(const char *name)
{
    tube t;
    size_t i;

    for (i = 0; i < tubes.used; i++) {
        t = tubes.items[i];
        if (strncmp(t->name, name, MAX_TUBE_NAME_LEN) == 0) return t;
    }
    return NULL;
}

tube
tube_find_or_make(const char *name)
{
    return tube_find(name) ? : make_and_insert_tube(name);
}

