// test_queue.c
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>
#include <assert.h>
#include "priority_queue.h"

typedef struct {
    int prio;
    int seq;   // ordine d'inserimento globale
} payload_t;

typedef struct {
    pq_t* q;
    int   id;
    int   count;     // quanti elementi produrre
    int   prio;      // priorità fissa per questo producer
    atomic_int* global_seq;
} producer_arg_t;

typedef struct {
    pq_t* q;
    int   id;
    int*  out_prios; // buffer condiviso delle priorità estratte
    int*  out_seqs;  // buffer condiviso dei seq estratti
    int*  out_len;   // lunghezza attuale del buffer
    mtx_t* out_mtx;  // mutex per proteggere i buffer
} consumer_arg_t;

static void tiny_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    thrd_sleep(&ts, NULL);
}

static int producer_fn(void* arg_) {
    producer_arg_t* arg = (producer_arg_t*)arg_;
    for (int i = 0; i < arg->count; ++i) {
        payload_t* p = (payload_t*)malloc(sizeof(payload_t));
        if (!p) abort();
        p->prio = arg->prio;
        p->seq  = atomic_fetch_add(arg->global_seq, 1);
        pq_push(arg->q, p, arg->prio);
        tiny_sleep_ms(1 + (p->seq % 3)); // jitter
    }
    return 0;
}

static int consumer_fn(void* arg_) {
    consumer_arg_t* arg = (consumer_arg_t*)arg_;
    for (;;) {
        payload_t* p = (payload_t*)pq_pop(arg->q);
        if (p == NULL) break; // coda chiusa e vuota
        mtx_lock(arg->out_mtx);
        int i = arg->out_len[0];
        arg->out_prios[i] = p->prio;
        arg->out_seqs[i]  = p->seq;
        arg->out_len[0] = i + 1;
        mtx_unlock(arg->out_mtx);
        free(p);
        tiny_sleep_ms(1);
    }
    return 0;
}

int main(void) {
    // ============ TEST 0: try_pop su coda vuota ============
    {
        pq_t* q0 = pq_create(3);
        assert(q0 != NULL);
        void* x = pq_try_pop(q0);
        assert(x == NULL);
        pq_close(q0);
        pq_destroy(q0);
        printf("[OK] try_pop() su coda vuota restituisce NULL\n");
    }

    // ============ TEST 1: concorrenza + FIFO intra-priority ============
    const int LEVELS = 3;
    pq_t* q = pq_create(LEVELS);
    if (!q) { fprintf(stderr, "pq_create fallita\n"); return 1; }

    const int N_PER_PRODUCER = 100;
    const int N_PRODUCERS = 3;
    const int TOTAL = N_PER_PRODUCER * N_PRODUCERS;

    atomic_int global_seq = ATOMIC_VAR_INIT(0);

    thrd_t prod[N_PRODUCERS];
    producer_arg_t parg[N_PRODUCERS];

    for (int i = 0; i < N_PRODUCERS; ++i) {
        parg[i].q = q;
        parg[i].id = i;
        parg[i].count = N_PER_PRODUCER;
        parg[i].prio = i; // 0,1,2
        parg[i].global_seq = &global_seq;
        assert(thrd_create(&prod[i], producer_fn, &parg[i]) == thrd_success);
    }

    // consumer
    const int N_CONS = 2;
    thrd_t cons[N_CONS];
    int* out_prios = (int*)calloc(TOTAL, sizeof(int));
    int* out_seqs  = (int*)calloc(TOTAL, sizeof(int));
    int out_len = 0;
    mtx_t out_mtx;
    mtx_init(&out_mtx, mtx_plain);

    consumer_arg_t carg[N_CONS];
    for (int i = 0; i < N_CONS; ++i) {
        carg[i].q = q;
        carg[i].id = i;
        carg[i].out_prios = out_prios;
        carg[i].out_seqs  = out_seqs;
        carg[i].out_len = &out_len;
        carg[i].out_mtx = &out_mtx;
        assert(thrd_create(&cons[i], consumer_fn, &carg[i]) == thrd_success);
    }

    // aspetta i producer
    for (int i = 0; i < N_PRODUCERS; ++i) thrd_join(prod[i], NULL);

    // chiudi coda e aspetta i consumer
    pq_close(q);
    for (int i = 0; i < N_CONS; ++i) thrd_join(cons[i], NULL);

    printf("Estratti %d elementi (attesi %d)\n", out_len, TOTAL);
    assert(out_len == TOTAL);

    // Verifica: FIFO intra-priority (per ogni priorità, i seq devono essere non-decrescenti)
    int last_seq[LEVELS];
    for (int p = 0; p < LEVELS; ++p) last_seq[p] = -1;

    for (int i = 0; i < out_len; ++i) {
        int p = out_prios[i];
        int s = out_seqs[i];
        if (s < last_seq[p]) {
            fprintf(stderr, "FIFO violata per priorità %d: seq %d dopo %d (pos %d)\n",
                    p, s, last_seq[p], i);
            assert(0 && "FIFO intra-priority violata");
        }
        last_seq[p] = s;
    }
    printf("[OK] FIFO intra-priority verificata in presenza di concorrenza\n");

    free(out_prios);
    free(out_seqs);
    mtx_destroy(&out_mtx);
    pq_destroy(q);

    // ============ TEST 2: priorità non-crescente quando la coda è già piena ============
    {
        pq_t* q2 = pq_create(LEVELS);
        assert(q2);

        // Pre-carica elementi: nessun consumer/produttore in esecuzione
        const int BATCH = 120;
        for (int i = 0; i < BATCH; ++i) {
            payload_t* p = malloc(sizeof(*p));
            assert(p);
            // mescola un po' le priorità
            p->prio = (i % LEVELS); // 0,1,2,0,1,2,...
            p->seq  = i;
            pq_push(q2, p, p->prio);
        }

        // Ora consuma tutto in single-thread (ordine deterministico)
        int last_prio = 999;
        for (int i = 0; i < BATCH; ++i) {
            payload_t* p = (payload_t*)pq_pop(q2);
            assert(p != NULL);
            // priorità devono essere non-crescenti (2..2, poi 1..1, poi 0..0)
            if (p->prio > last_prio) {
                fprintf(stderr, "Ordine priorità violato su coda pre-riempita: %d -> %d\n",
                        last_prio, p->prio);
                assert(0 && "estrazione non rispetta priorità non-crescente su coda piena");
            }
            last_prio = p->prio;
            free(p);
        }
        pq_close(q2);
        pq_destroy(q2);

        printf("[OK] ordine di priorità non-crescente verificato su coda pre-riempita\n");
    }

    printf("TUTTI I TEST SONO PASSATI ✅\n");
    return 0;
}