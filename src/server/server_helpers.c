#include "server.h"

extern server_context_t *ctx;

void lock_emergencies() {
	mtx_lock(&ctx->active_emergencies->mutex);
}

void unlock_emergencies() {
	mtx_unlock(&ctx->active_emergencies->mutex);
}

void lock_rescuers() {
	mtx_lock(&ctx->rescuers->mutex);
}

void unlock_rescuers() {
	mtx_unlock(&ctx->rescuers->mutex);
}

void lock_system() {
	mtx_lock(&ctx->rescuers->mutex);
	mtx_lock(&ctx->active_emergencies->mutex);
}

void unlock_system() {
	mtx_unlock(&ctx->active_emergencies->mutex);
	mtx_unlock(&ctx->rescuers->mutex);
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
	change_bresenham_trajectory(t->trajectory, t->x, t->y, new_x, new_y);
}

void remove_twin_from_its_emergency(rescuer_digital_twin_t *t) {
	if (!t || !t->emergency) return;
    int i = get_twin_index_by_id(t->id, t->emergency->rescuer_twins, t->emergency->rescuer_count);
    if (i == INDEX_NOT_FOUND) return;
    t->emergency->rescuer_twins[i] = NULL;
}