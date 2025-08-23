#include "priority_queue.h"
#include <stdlib.h>

// inserisce un nodo in fondo alla lista
static void list_push_back(list_t* list, node_t* n){
    n->prev = list->tail;
    n->next = NULL;
    if (list->tail) 
        list->tail->next = n; 
    else 
        list->head = n;
    list->tail = n;
    list->count++;
}

// rimuove un nodo dalla cima della lista
static node_t* list_pop_front(list_t* list){
    node_t* n = list->head;
    if(!n) return NULL;
    list->head = n->next;
    if(list->head) 
        list->head->prev = NULL; 
    else 
        list->tail = NULL;
    list->count--;
    return n;
}

// funzioni da usare all'esterno

pq_t* pq_create(int levels){
    if(levels==0) return NULL;
    pq_t* q = malloc(sizeof(pq_t));
    check_error_memory_allocation(q);
    q->levels = levels;
    q->lists = calloc(levels,sizeof(list_t));
    check_error_memory_allocation(q->lists);
    q->size = 0;
    q->closed = false;
    mtx_init(&q->mtx, mtx_plain);
    cnd_init(&q->not_empty);
    return q;
}

void pq_destroy(pq_t* q){
    if(!q) return;
    for(int i = 0; i < q->levels; i++){
        node_t* n = q->lists[i].head;
        while(n){
            node_t* next=n->next;
            free(n);
            n=next;
        }
    }
    free(q->lists);
    cnd_destroy(&q->not_empty);
    mtx_destroy(&q->mtx);
    free(q);
}

void pq_close(pq_t* q){                               
    if(!q) return;
    mtx_lock(&q->mtx);
    q->closed = true;
    cnd_broadcast(&q->not_empty);  // sveglia tutti i waiter se la coda è stata chiusa
    mtx_unlock(&q->mtx);
}

void pq_push(pq_t* q, void* item, int prio){
    if(!q) return;
    node_t* n = malloc(sizeof(node_t));       
    check_error_memory_allocation(n);                           
    n->item=item; n->prev=n->next=NULL;
    mtx_lock(&q->mtx);
    // non accetto push su coda chiusa
    if(q->closed){
        mtx_unlock(&q->mtx);
        free(n);
        return; 
    }
    if(prio >= q->levels) prio = q->levels-1;
    if(prio < 0) prio = 0;

    list_push_back(&q->lists[prio], n);
    q->size++;
    cnd_signal(&q->not_empty);
    mtx_unlock(&q->mtx);
}

void* pq_pop(pq_t* q){
    if(!q) return NULL;
    mtx_lock(&q->mtx);
    // aspetto che ci sia qualcosa nella coda o che venga chiusa
    while(q->size == 0 && !q->closed){
        cnd_wait(&q->not_empty, &q->mtx);
    }
    // se la coda è vuota e chiusa, esco subito
    if(q->size == 0 && q->closed){
        mtx_unlock(&q->mtx);
        return NULL;
    }
    node_t* n = NULL;
    for(int i = q->levels - 1; i >= 0; --i){
        n = list_pop_front(&q->lists[i]);
        if(n) break;
    }
    q->size--;
    mtx_unlock(&q->mtx);
    void* item = n->item;
    free(n);
    return item;
}

void* pq_try_pop(pq_t* q){
    if(!q) return NULL;
    mtx_lock(&q->mtx);
    if(q->size==0){ mtx_unlock(&q->mtx); return NULL; }
    node_t* n=NULL;
    for(int i = q->levels - 1; i >= 0; --i){
        n=list_pop_front(&q->lists[i]);
        if(n) break;
    }
    q->size--;
    mtx_unlock(&q->mtx);
    void* item=n->item;
    free(n);
    return item;
}
