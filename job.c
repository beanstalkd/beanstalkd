#include "dat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint64 next_id = 1;

static int cur_prime = 0;

static Job *all_jobs_init[12289] = {0};
static Job **all_jobs = all_jobs_init;
static size_t all_jobs_cap = 12289; /* == primes[0] */
static size_t all_jobs_used = 0;

static int hash_table_was_oom = 0;

static void rehash(int);

static int
_get_job_hash_index(uint64 job_id)
{
    return job_id % all_jobs_cap;
}

static void
store_job(Job *j)
{
    int index = 0;

    index = _get_job_hash_index(j->r.id);

    j->ht_next = all_jobs[index];
    all_jobs[index] = j;
    all_jobs_used++;

    /* accept a load factor of 4 */
    if (all_jobs_used > (all_jobs_cap << 2)) rehash(1);
}

static void
rehash(int is_upscaling)
{
    Job **old = all_jobs;
    size_t old_cap = all_jobs_cap, old_used = all_jobs_used, i;
    int old_prime = cur_prime;
    int d = is_upscaling ? 1 : -1;

    if (cur_prime + d >= NUM_PRIMES) return;
    if (cur_prime + d < 0) return;
    if (is_upscaling && hash_table_was_oom) return;

    cur_prime += d;

    all_jobs_cap = primes[cur_prime];
    all_jobs = calloc(all_jobs_cap, sizeof(Job *));
    if (!all_jobs) {
        twarnx("Failed to allocate %zu new hash buckets", all_jobs_cap);
        hash_table_was_oom = 1;
        cur_prime = old_prime;
        all_jobs = old;
        all_jobs_cap = old_cap;
        all_jobs_used = old_used;
        return;
    }
    all_jobs_used = 0;
    hash_table_was_oom = 0;

    for (i = 0; i < old_cap; i++) {
        while (old[i]) {
            Job *j = old[i];
            old[i] = j->ht_next;
            j->ht_next = NULL;
            store_job(j);
        }
    }
    if (old != all_jobs_init) {
        free(old);
    }
}

Job *
job_find(uint64 job_id)
{
    int index = _get_job_hash_index(job_id);
    Job *jh = all_jobs[index];

    while (jh && jh->r.id != job_id)
        jh = jh->ht_next;

    return jh;
}

Job *
allocate_job(int body_size)
{
    Job *j;

    j = malloc(sizeof(Job) + body_size);
    if (!j) {
        twarnx("OOM");
        return (Job *) 0;
    }

    memset(j, 0, sizeof(Job));
    j->r.created_at = nanoseconds();
    j->r.body_size = body_size;
    j->body = (char *)j + sizeof(Job);
    job_list_reset(j);
    return j;
}

Job *
make_job_with_id(uint32 pri, int64 delay, int64 ttr,
                 int body_size, Tube *tube, uint64 id)
{
    Job *j;

    j = allocate_job(body_size);
    if (!j) {
        twarnx("OOM");
        return (Job *) 0;
    }

    if (id) {
        j->r.id = id;
        if (id >= next_id) next_id = id + 1;
    } else {
        j->r.id = next_id++;
    }
    j->r.pri = pri;
    j->r.delay = delay;
    j->r.ttr = ttr;

    store_job(j);

    TUBE_ASSIGN(j->tube, tube);

    return j;
}

static void
job_hash_free(Job *j)
{
    Job **slot;

    slot = &all_jobs[_get_job_hash_index(j->r.id)];
    while (*slot && *slot != j) slot = &(*slot)->ht_next;
    if (*slot) {
        *slot = (*slot)->ht_next;
        --all_jobs_used;
    }

    // Downscale when the hashmap is too sparse
    if (all_jobs_used < (all_jobs_cap >> 4)) rehash(0);
}

void
job_free(Job *j)
{
    if (j) {
        TUBE_ASSIGN(j->tube, NULL);
        if (j->r.state != Copy) job_hash_free(j);
    }

    free(j);
}

void
job_setpos(void *j, size_t pos)
{
    ((Job *)j)->heap_index = pos;
}

int
job_pri_less(void *ja, void *jb)
{
    Job *a = (Job *)ja;
    Job *b = (Job *)jb;
    if (a->r.pri < b->r.pri) return 1;
    if (a->r.pri > b->r.pri) return 0;
    return a->r.id < b->r.id;
}

int
job_delay_less(void *ja, void *jb)
{
    Job *a = ja;
    Job *b = jb;
    if (a->r.deadline_at < b->r.deadline_at) return 1;
    if (a->r.deadline_at > b->r.deadline_at) return 0;
    return a->r.id < b->r.id;
}

Job *
job_copy(Job *j)
{
    if (!j)
        return NULL;

    Job *n = malloc(sizeof(Job) + j->r.body_size);
    if (!n) {
        twarnx("OOM");
        return (Job *) 0;
    }

    memcpy(n, j, sizeof(Job) + j->r.body_size);
    job_list_reset(n);

    n->file = NULL; /* copies do not have refcnt on the wal */

    n->tube = 0; /* Don't use memcpy for the tube, which we must refcount. */
    TUBE_ASSIGN(n->tube, j->tube);

    /* Mark this job as a copy so it can be appropriately freed later on */
    n->r.state = Copy;

    return n;
}

const char *
job_state(Job *j)
{
    if (j->r.state == Ready) return "ready";
    if (j->r.state == Reserved) return "reserved";
    if (j->r.state == Buried) return "buried";
    if (j->r.state == Delayed) return "delayed";
    return "invalid";
}

// job_list_reset detaches head from the list,
// marking the list starting in head pointing to itself.
void
job_list_reset(Job *head)
{
    head->prev = head;
    head->next = head;
}

int
job_list_is_empty(Job *head)
{
    return head->next == head && head->prev == head;
}

Job *
job_list_remove(Job *j)
{
    if (!j) return NULL;
    if (job_list_is_empty(j)) return NULL; /* not in a doubly-linked list */

    j->next->prev = j->prev;
    j->prev->next = j->next;

    job_list_reset(j);

    return j;
}

void
job_list_insert(Job *head, Job *j)
{
    if (!job_list_is_empty(j)) return; /* already in a linked list */

    j->prev = head->prev;
    j->next = head;
    head->prev->next = j;
    head->prev = j;
}

/* for unit tests */
size_t
get_all_jobs_used()
{
    return all_jobs_used;
}
