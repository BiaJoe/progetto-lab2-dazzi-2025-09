#include "server.h"

int index;
mtx_t slot_mtx;

static void open_slot(emergency_t *e) {
	mtx_lock(&slot_mtx);
	ctx->active_emergencies->array[index] = e;
	mtx_unlock(&slot_mtx);
}

static void close_slot() {
	mtx_lock(&slot_mtx);
	ctx->active_emergencies->array[index] = NULL;
	mtx_unlock(&slot_mtx);
}

int thread_worker(void *arg){
	// l'indice personal del worker, è quello con cui accede alla sua casella di emergenza
	index = atomic_fetch_add(&ctx->active_emergencies->next_tw_index, 1);
	if (index >= WORKER_THREADS_NUMBER) {
		log_error_and_exit(close_server, "non abbastanza slot di emergenze per i thread workers");
	}
	
	// variabili per dare nomi più semplici alle cose
	slot_mtx = ctx->active_emergencies->mutex;
	pq_t *queue = ctx->emergency_queue;

	while(!atomic_load(&ctx->server_must_stop)){	
		emergency_t *e = (emergency_t *)pq_pop(queue);
		if (!e) break;
		open_slot(e);

		if (!find_rescuers_logging_blocking(e)) {
			timeout_active_emergency_logging_blocking(e);
			close_slot();
			continue;
		}

		// i rescuer stanno arrivando, mi metto in attesa

		mtx_lock(&e->mutex);
		while(e->status != PAUSED && e->status != COMPLETED && !atomic_load(&ctx->server_must_stop)) {
			cnd_wait(&e->cond, &e->mutex);
		}
		mtx_unlock(&e->mutex);

		if (atomic_load(&ctx->server_must_stop)) break;
		
		// in base a che stato ha e prendiamo provvedimenti

		close_slot();

	}		

	return 0;
}

void timeout_active_emergency_logging_blocking(emergency_t *e){
	mtx_lock(&e->mutex);
	e->status = TIMEOUT;
	mtx_unlock(&e->mutex);
	log_event(e->id, EMERGENCY_STATUS, "(%d, %d) %s è stata messa in TIMEOUT perchè non abbastanza rescuers arriveranno in tempo!", e->x, e->y, e->type->emergency_desc);
}

bool find_rescuers_logging_blocking(emergency_t *e) {
	rescuer_digital_twin_t *candidates[e->rescuer_count];
	mtx_lock(&e->mutex);
	
	mtx_unlock(&e->mutex);
}