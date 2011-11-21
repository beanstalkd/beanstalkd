#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dat.h"

#define EMPTY_TRASH_JOBS 5

struct ms tubes;
struct ms trash_heaps;

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

    t->ready.cap = 0;
    t->delay.cap = 0;
    t->ready.len = 0;
    t->delay.len = 0;
    t->ready.data = NULL;
    t->delay.data = NULL;

    t->ready.less = job_pri_less;
    t->delay.less = job_delay_less;
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

void tube_clear(tube t)
{ 
    Heap *heaps[]={&t->ready,&t->delay}; //These are the heaps we need to clear
    int count=sizeof(heaps)/sizeof(Heap*); //counter for the for loop
    int i=0;
    
    for(i=0;i<count;i++)
    {
         //Instead of deleting the heaps, we move them to the trash
        //and delete them in the main event handler
        Heap * trash=malloc(sizeof(struct Heap));
        
        //Copy the current tube definition to the trash
        memcpy(trash,heaps[i],sizeof(struct Heap));
        
        //"Empty" the heaps
        heaps[i]->cap=heaps[i]->len=0;
        heaps[i]->data=NULL;//we can se this to null because the data is now in the trash
        
        //Move the trash tube the trash list
        ms_append(&trash_heaps,trash);
    }
    
    /*clear the stats for the tube*/
    t->stat.buried_ct=0;
    t->stat.pause_ct=0;
    t->stat.total_delete_ct=0;
    t->stat.total_jobs_ct=0;
    t->stat.urgent_ct=0;
    t->stat.waiting_ct=0;
    
    ms_clear(&t->waiting);
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

void
tube_empty_trash(){
    Heap * heap;
    while(heap=ms_take(&trash_heaps)){
        job j;
        int i=0;
        while((j=heapremove(heap,0))!=0 && i++<EMPTY_TRASH_JOBS) 
        {
            job_remove(j);
            j->r.state = Invalid;
            job_free(j);
        }
        
        if(j)
        {
            //this was not the last job in the heap, add the heap again for future use
           ms_append(&trash_heaps,heap);
        }
        else
        {
            //no more jobs in the heap, delete it
            free(heap->data);
            free(heap);
        }
    }
}