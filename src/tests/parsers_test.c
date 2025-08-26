// src/tests/parsers_test.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "config_server.h"   // server_logging_config, priority_lookup_table, priority_count, *_CONF
#include "log.h"
#include "parsers.h"
#include "structs.h"

// mini sleep per dare tempo al thread logger
static void tiny_sleep_ms(long ms) {
    struct timespec t = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    thrd_sleep(&t, NULL);
}

static void print_environment(const environment_t *e) {
    printf("\n=== ENVIRONMENT ===\n");
    printf("queue_name: %s\n", e->queue_name);
    printf("height    : %d\n", e->height);
    printf("width     : %d\n", e->width);
}

static void print_rescuers(const rescuers_t *r) {
    printf("\n=== RESCUERS TYPES (%d) ===\n", r->count);
    for (int i = 0; i < r->count; ++i) {
        const rescuer_type_t *t = r->types[i];
        if (!t) continue;
        printf("- [%d] name=%s  amount=%d  speed=%d  base=(%d,%d)\n",
               i, t->rescuer_type_name, t->amount, t->speed, t->x, t->y);

        // facoltativo: mostra i primi 3 twins (se esistono)
        int show = t->amount < 3 ? t->amount : 3;
        for (int k = 0; k < show; ++k) {
            const rescuer_digital_twin_t *tw = t->twins[k];
            if (!tw) break;
            printf("    twin#%d at (%d,%d) status=%d\n", tw->id, tw->x, tw->y, (int)tw->status);
        }
        if (t->amount > show) printf("    ... (+%d twins)\n", t->amount - show);
    }
}

static void print_emergencies(const emergencies_t *em) {
    printf("\n=== EMERGENCY TYPES (%d) ===\n", em->count);
    for (int i = 0; i < em->count; ++i) {
        const emergency_type_t *et = em->types[i];
        if (!et) continue;
        printf("- [%d] name=%s  priority=%d  requests=%d\n",
               i, et->emergency_desc, (int)et->priority, et->rescuers_req_number);
        for (int j = 0; j < et->rescuers_req_number; ++j) {
            const rescuer_request_t *rq = et->rescuers[j];
            if (!rq || !rq->type) continue;
            printf("    • %s: count=%d  time_to_manage=%d\n",
                   rq->type->rescuer_type_name, rq->required_count, rq->time_to_manage);
        }
    }
}

int main(void) {
    // 1) logger come SERVER (thread + coda)
    if (log_init(LOG_ROLE_SERVER, server_logging_config) != 0) {
        fprintf(stderr, "log_init failed\n");
        return 1;
    }

    // 2) parse environment
    environment_t *env = parse_env(ENV_CONF);
    print_environment(env);

    // 3) parse rescuers (richiede i limiti dell'env)
    rescuers_t *resc = parse_rescuers(RESCUERS_CONF, env->width, env->height);
    print_rescuers(resc);

    // 4) parse emergency types (richiede rescuers + LUT priorità)
    emergencies_t *em = parse_emergencies(EMERGENCY_TYPES_CONF, resc, priority_lookup_table, priority_count);
    print_emergencies(em);

    // piccolo delay per far drenare il logger
    tiny_sleep_ms(20);
    log_close();

    // Nota: qui non libero la memoria (test semplice, vita breve del processo)
    // Se vuoi, posso aggiungere una free() rigorosa per tutte le strutture.

    return 0;
}