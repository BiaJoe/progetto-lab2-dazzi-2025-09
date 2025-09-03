#include "server.h"

extern server_context_t *ctx;

int thread_clock(void *arg){
	log_register_this_thread("CLOCK");
	while(!atomic_load(&ctx->server_must_stop)){
		thrd_sleep(&(ctx->clock->tick_time), NULL); 	// attendo un tick di tempo del server
		lock_server_clock();							// blocco il mutex per il tempo del server
		tick(); 										// il sterver ha tickato
		cnd_broadcast(&ctx->clock->updated);			// segnalo al thread updater che il tempo è stato aggiornato
		unlock_server_clock();
	}
	// un ultimo tick prima di morire
	lock_server_clock();					
	tick(); 								
	cnd_broadcast(&ctx->clock->updated);	
	unlock_server_clock();
	return 0;
}

int get_current_tick_count() {
	return (int) atomic_load(&ctx->clock->tick_count_since_start);
}
 
void tick(){
	ctx->clock->tick = true; 				
	int tick_number = atomic_fetch_add(&ctx->clock->tick_count_since_start, 1);
	char *tick_string = (tick_number % 2 == 0) ? "tick" : "tack";
	log_event(AUTOMATIC_LOG_ID, SERVER_CLOCK, tick_string);
}

void untick(){
	ctx->clock->tick = false; 																				
}

void wait_for_a_tick(){
	cnd_wait(&ctx->clock->updated, &ctx->clock->mutex); 	// attendo che il thread clock mi segnali che il tempo è stato aggiornato
}

int server_is_ticking(){
	return ctx->clock->tick;
}

void lock_server_clock(){
	LOCK(ctx->clock->mutex);
}

void unlock_server_clock(){
	UNLOCK(ctx->clock->mutex);
}