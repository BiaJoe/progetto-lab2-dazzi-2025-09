
#include "server_utils.h"

// serve a mandare un rescuer in un posto che vogliamo
// la usano più file, wrappata in altre funzioni per facilitare il logging
void change_rescuer_digital_twin_destination(rescuer_digital_twin_t *t, int new_x, int new_y){
	if(!t || (t->x == new_x && t->y == new_y)) return; 		// se non è cambiata la destinazione non faccio nulla
	change_bresenham_trajectory(t->trajectory, t->x, t->y, new_x, new_y);
	t->is_travelling = YES;
	t->has_arrived = NO;
	t->time_left_before_it_can_leave_the_scene = INVALID_TIME;
}


// lock e unlock di strutture per facilitare la lettura del codice

void lock_rescuer_types(server_context_t *ctx){
	LOCK(ctx->rescuers_mutex);
}

void unlock_rescuer_types(server_context_t *ctx){
	UNLOCK(ctx->rescuers_mutex);
}

void lock_queue(emergency_queue_t *q){
	if(q) LOCK(q->mutex);
}

void unlock_queue(emergency_queue_t *q){
	if(q) UNLOCK(q->mutex);
}

void lock_list(emergency_list_t *l){
	if(l) LOCK(l->mutex);
}

void unlock_list(emergency_list_t *l){
	if(l) UNLOCK(l->mutex);
}

void lock_node(emergency_node_t *n){
	if(n) LOCK(n->mutex);									
}

void unlock_node(emergency_node_t *n){
	if(n) UNLOCK(n->mutex);
}
