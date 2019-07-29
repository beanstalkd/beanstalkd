#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct Ms tubes;

Tube *
make_tube(const char *name)
{
    Tube *t;

    t = new(Tube);
    if (!t) return NULL;

    t->name[MAX_TUBE_NAME_LEN - 1] = '\0';
    strncpy(t->name, name, MAX_TUBE_NAME_LEN - 1);
    if (t->name[MAX_TUBE_NAME_LEN - 1] != '\0')
        twarn("truncating tube name");

    t->ready.less = job_pri_less;
    t->delay.less = job_delay_less;
    t->ready.setpos = job_setpos;
    t->delay.setpos = job_setpos;

    Job j = {.tube = NULL};
    t->buried = j;
    t->buried.prev = t->buried.next = &t->buried;
    ms_init(&t->waiting, NULL, NULL);

    return t;
}

static void
tube_free(Tube *t)
{
    prot_remove_tube(t);
    free(t->ready.data);
    free(t->delay.data);
    ms_clear(&t->waiting);
    free(t);
}

void
tube_dref(Tube *t)
{
    if (!t) return;
    if (t->refs < 1)
        return twarnf("refs is zero for tube: %s", t->name);

    --t->refs;
    if (t->refs < 1)
        tube_free(t);
}

void
tube_iref(Tube *t)
{
    if (!t) return;
    ++t->refs;
}

static Tube *
make_and_insert_tube(const char *name)
{
    int r;
    Tube *t = NULL;

    t = make_tube(name);
    if (!t)
        return NULL;

    /* We want this global tube list to behave like "weak" refs, so don't
     * increment the ref count. */
    r = ms_append(&tubes, t);
    if (!r)
        return tube_dref(t), (Tube *) 0;

    return t;
}

Tube *
tube_find(const char *name)
{
    size_t i;

    for (i = 0; i < tubes.len; i++) {
        Tube *t = tubes.items[i];
        if (strncmp(t->name, name, MAX_TUBE_NAME_LEN) == 0)
            return t;
    }
    return NULL;
}

Tube *
tube_find_or_make(const char *name)
{
    return tube_find(name) ? : make_and_insert_tube(name);
}

