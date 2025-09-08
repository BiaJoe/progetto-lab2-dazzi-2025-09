#include "server.h"

extern server_context_t *ctx;

void lock_emergencies() {
	log_event(AUTOMATIC_LOG_ID, DEBUG, "adesso blocco emergenze attive");
	mtx_lock(&ctx->active_emergencies->mutex);
	log_event(AUTOMATIC_LOG_ID, DEBUG, "emergenze bloccate");
}

void unlock_emergencies() {
	log_event(AUTOMATIC_LOG_ID, DEBUG, "sblocco emergenze attive");
	mtx_unlock(&ctx->active_emergencies->mutex);
	log_event(AUTOMATIC_LOG_ID, DEBUG, "emergenze Sbloccate");

}

void lock_rescuers() {
	log_event(AUTOMATIC_LOG_ID, DEBUG, "adesso blocco rescuers");
	mtx_lock(&ctx->rescuers->mutex);
	log_event(AUTOMATIC_LOG_ID, DEBUG, "rescuers bloccati");
}

void unlock_rescuers() {
	log_event(AUTOMATIC_LOG_ID, DEBUG, "Sblocco rescuers");
	mtx_unlock(&ctx->rescuers->mutex);
	log_event(AUTOMATIC_LOG_ID, DEBUG, "rescuers Sbloccati");
}

void lock_system() {
	log_event(AUTOMATIC_LOG_ID, DEBUG, "adesso BLOCCO SISTEMA");
	mtx_lock(&ctx->rescuers->mutex);
	mtx_lock(&ctx->active_emergencies->mutex);
	log_event(AUTOMATIC_LOG_ID, DEBUG, "SISTEMA BLOCCATO");

}

void unlock_system() {
	log_event(AUTOMATIC_LOG_ID, DEBUG, "SBLOCCO SISTEMA");
	mtx_unlock(&ctx->active_emergencies->mutex);
	mtx_unlock(&ctx->rescuers->mutex);
	log_event(AUTOMATIC_LOG_ID, DEBUG, "SISTEMA SBLOCCATO");
}


// ritorna > 0 se la prima è più grande
// ritorna < 0 se la seconda è più grande
// ritorna = 0 se sono uguali
int compare_priorities(short a, short b) {
	int aL = priority_to_level(a);
	int bL = priority_to_level(b);
	return (aL - bL);
}

void change_rescuer_digital_twin_destination_blocked(rescuer_digital_twin_t *t, int new_x, int new_y){
	if(!t) return; 
	if((t->x == new_x && t->y == new_y)) {
		t->has_arrived = true;
		return;
	}
	t->has_arrived = false;
	t->time_left_before_leaving = INVALID_TIME;
	LOG_RESCUER_STATUS_DEBUG(t, "pos cambiata");
	change_bresenham_trajectory(t->trajectory, t->x, t->y, new_x, new_y);
}

void remove_twin_from_its_emergency(rescuer_digital_twin_t *t) {
	if (!t || !t->emergency) return;
    int i = get_twin_index_by_id(t->id, t->emergency->rescuer_twins, t->emergency->rescuer_count);
    if (i == INDEX_NOT_FOUND) return;
    t->emergency->rescuer_twins[i] = NULL;
	LOG_RESCUER_STATUS_DEBUG(t, "rimosso da emergenza");
	LOG_EMERGENCY_STATUS_DEBUG(t->emergency, "rimosso un rescuer");
}