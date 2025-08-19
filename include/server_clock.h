
#ifndef SERVER_CLOCK_H
#define SERVER_CLOCK_H

#include "server_utils.h"

// tick del server, 1 secondo, ma può essere cambiato per aumentare/diminuire la velocità del server
#define SERVER_TICK_INTERVAL_SECONDS 1
#define SERVER_TICK_INTERVAL_NANOSECONDS 0

int server_clock(void *arg);

void tick(server_context_t *ctx);
void untick(server_context_t *ctx);
void wait_for_a_tick(server_context_t *ctx);
int server_is_ticking(server_context_t *ctx);


// lock accessibili solo a clock e a chi altro ha #include "server_clock.h" volutamente 

void lock_server_clock(server_context_t *ctx);
void unlock_server_clock(server_context_t *ctx);

#endif