#include "server.h"

// ----------- funzioni per il thread clock e chi deve avere accesso ai tick del server ----------- 

static const struct timespec server_tick_time = {
	.tv_sec = SERVER_TICK_INTERVAL_SECONDS,
	.tv_nsec = SERVER_TICK_INTERVAL_NANOSECONDS
};

int thread_clock(void *arg){
	server_context_t *ctx = arg;
	while(!ctx->server_must_stop){
		thrd_sleep(&server_tick_time, NULL); 	// attendo un tick di tempo del server
		lock_server_clock(ctx);					// blocco il mutex per il tempo del server
		tick(ctx); 								// il sterver ha tickato
		cnd_broadcast(&ctx->clock->updated);	// segnalo al thread updater che il tempo è stato aggiornato
		unlock_server_clock(ctx);
	}
	return 0;
}
 
void tick(server_context_t *ctx){
	ctx->clock->tick = true; 						
	ctx->clock->tick_count_since_start++; 	// incremento il contatore dei tick del server perchè è appena avvenuto un tick														
}

void untick(server_context_t *ctx){
	ctx->clock->tick = false; 																				
}

void wait_for_a_tick(server_context_t *ctx){
	cnd_wait(&ctx->clock->updated, &ctx->clock->mutex); 	// attendo che il thread clock mi segnali che il tempo è stato aggiornato
}

int server_is_ticking(server_context_t *ctx){
	return ctx->clock->tick;
}

void lock_server_clock(server_context_t *ctx){
	LOCK(ctx->clock->mutex);
}

void unlock_server_clock(server_context_t *ctx){
	UNLOCK(ctx->clock->mutex);
}