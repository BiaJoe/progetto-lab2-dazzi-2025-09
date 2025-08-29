#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stddef.h>
#include <threads.h>
#include <stdbool.h> 
#include "errors.h"

// dichiaro le struttture nell'header per semplicità
// in uno scenario reale di libreria sarebbe un'opzione dichiarare una struttura opaca
// e dichiararle nel file .c

typedef struct node {
    struct node* prev;
    struct node* next;
    void* item;
} node_t;

typedef struct {
    struct node* head;
    struct node* tail;
    int count;
} list_t;

typedef struct pq {
    int levels;
    list_t* lists;
    int size;
    mtx_t mtx;
    cnd_t not_empty;
    bool  closed;   // se la coda è stata chiusa (non si possono più inserire item)
} pq_t;


// Crea una coda con N livelli di priorità (0 = bassa, levels-1 = alta)
pq_t* pq_create(int levels);
void  pq_destroy(pq_t* q);
void  pq_close(pq_t* q);
void  pq_push(pq_t* q, void* item, int prio); // prio = livello a cui inserire 0 - levels-1
void* pq_pop(pq_t* q);                        // bloccata in cnd_wait finchè la coda non ha qualcosa dentro o viene chiusa
void* pq_try_pop(pq_t* q);                    // non si blocca: NULL se non c'è niente
void* pq_extract_first(pq_t* q, bool (*pred)(void*));
void  pq_map(pq_t* q, void (*fn)(void*));

#endif

