#include "server.h"

// ----------- funzioni per il thread updater -------------

// funzione che gira ogni tick del clock
// fa l'update di tutte le strutture del server
// blocca tutti i mutex principali 
// per non fare inserimenti o rimozioni dalle queue e le liste durante l'update
// blocca i singoli nodi quando li sta usando
int thread_updater(void *arg){
	while(!atomic_load(&ctx->server_must_stop)){		
		 lock_server_clock(ctx);
        while (!server_is_ticking(ctx) && !atomic_load(&ctx->server_must_stop))
            wait_for_a_tick(ctx);  
        if (atomic_load(&ctx->server_must_stop)) { 
            unlock_server_clock(ctx);
            break;
        }
        untick(ctx);
        unlock_server_clock(ctx);

		log_event(AUTOMATIC_LOG_ID, SERVER_UPDATE, "🔄  aggiornamento #%d del server iniziato...", ctx->clock->tick_count_since_start);

		update_rescuers_states_logging();
		update_active_emergencies_states_logging();
		update_waiting_emergencies_states();
		remove_expired_waiting_emergencies_logging();
		promote_needing_emergencies();
		
		log_event(AUTOMATIC_LOG_ID, SERVER_UPDATE, "👍  aggiornamento #%d del server finito!", ctx->clock->tick_count_since_start);
	}
	return 0;
}

void update_rescuers_states_logging() {
	int amount = ctx->rescuers->count;
	for(int i = 0; i < amount; i++){					// aggiorno le posizioni dei gemelli rescuers
		rescuer_type_t *r = ctx->rescuers->types[i];
		if(r == NULL) continue; 						// se il rescuer type è NULL non faccio nulla (precauzione)
		for(int j = 0; j < r->amount; j++){
			rescuer_digital_twin_t *dt = r->twins[j];
			update_rescuer_digital_twin_state_logging_blocking(dt, MIN_X_COORDINATE_ABSOLUTE_VALUE, MIN_Y_COORDINATE_ABSOLUTE_VALUE, ctx->enviroment->height, ctx->enviroment->width);
		}
	}
}

static bool twin_shouldnt_move(rescuer_digital_twin_t *t) {
	return (
		!t || 
		t->status == IDLE ||
		t->status == ON_SCENE && (t->time_left_before_leaving > 0)
	);
}

bool update_rescuer_digital_twin_state_logging_blocking(rescuer_digital_twin_t *t, int minx, int miny, int height, int width){
	if (!t) return false;

	mtx_lock(&t->mutex);
	if (twin_shouldnt_move(t)){ // se non deve muoversi non faccio nulla
		mtx_unlock(&t->mutex);
		return false; 			// la posizione non va aggiornata
	}

	// se deve lasciare la scena cambio la sua destinazione, ma non esco perchè la posizione va ancora aggiornata
	if(t->status == ON_SCENE && --(t->time_left_before_leaving) <= 0) {
		atomic_fetch_sub(&t->emergency->rescuers_not_done_yet, 1);
		send_rescuer_digital_twin_back_to_base_logging_blocked(t);
		t->emergency = NULL; // il rescuer non ha più niente da cotribuire in questa emergenza
	}

	t->is_travelling = true; // ci accede il gemello vede che sta viaggiando
	
	int xA = t->x;
	int yA = t->y;
	int cells_to_walk_on_the_X_axis = 0;
	int cells_to_walk_on_the_Y_axis = 0;
	
	bool we_have_arrived = compute_bresenham_step(
		t->x,
		t->y,
		t->trajectory,
		t->rescuer->speed,
		&cells_to_walk_on_the_X_axis,
		&cells_to_walk_on_the_Y_axis
	);

	t->x += cells_to_walk_on_the_X_axis;		// faccio il passo
	t->y += cells_to_walk_on_the_Y_axis;

	if (t->x < minx || t->x > width || t->y < miny || t->y > height) {
		mtx_unlock(&t->mutex);
		log_error_and_exit(close_server, "Rescuer %s #%d uscito dalla mappa (%d, %d)", t->rescuer->rescuer_type_name, t->id, t->x, t->y);
	}

	if (!we_have_arrived){
		log_event(t->id, RESCUER_TRAVELLING_STATUS, "📍 %s [%d] : [%d, %d] -> [%d, %d] il rescuer si è spostato", t->rescuer->rescuer_type_name, t->id, xA, yA, t->x, t->y);
		mtx_unlock(&t->mutex);
		return true; // ho aggiornato la posizione
	}

	// siamo arrivati! non stiamo più viaggiando	
	switch (t->status) {
		case EN_ROUTE_TO_SCENE:
			t->time_left_before_leaving = t->time_to_manage;
			t->status = ON_SCENE;
			atomic_fetch_sub(&t->emergency->rescuers_not_arrived_yet, 1);	// un rescuer in più è arrivato!
			log_event(t->id, RESCUER_STATUS, "🚨 %s [%d] -> [%d, %d] il rescuer è arrivato alla scena dell'emergenza  !!!!", t->rescuer->rescuer_type_name, t->id, t->x, t->x);
			mtx_unlock(&t->mutex);
			return true;		
		case RETURNING_TO_BASE:
			t->status = IDLE;
			log_event(t->id, RESCUER_STATUS, "⛑️ %s [%d] -> [%d, %d] il rescuer è tornato sano e salvo alla base  :)", t->rescuer->rescuer_type_name, t->id, t->x, t->x);
			mtx_unlock(&t->mutex);
			return true;
		default: 
			mtx_unlock(&t->mutex);
			log_error_and_exit(close_server, "spostamento rescuer dt");
	}	
	mtx_unlock(&t->mutex);
	return false; // non si arriva qui, si fallisce o riesce prima
}

void send_rescuer_digital_twin_back_to_base_logging_blocked(rescuer_digital_twin_t *t){			
	switch (t->status) {
		case IDLE: return;									// se è già alla base non faccio nulla
		case RETURNING_TO_BASE: return;
		case ON_SCENE: 
			log_event(t->id, RESCUER_STATUS, "⬅ %s [%d] : ⚠️ [%d, %d] ... [%d, %d] 🏠 - il rescuer parte dalla scena dell'emergenza per tornare alla base", t->rescuer->rescuer_type_name, t->id, t->x, t-> y, t->rescuer->x, t->rescuer->y);
			break;
		case EN_ROUTE_TO_SCENE:
			log_event(t->id, RESCUER_STATUS, "⬅ %s [%d] : 🚀 [%d, %d] ... [%d, %d] 🏠 - il rescuer stava andando su una scena ma ora torna alla base", t->rescuer->rescuer_type_name, t->id, t->x, t-> y, t->rescuer->x, t->rescuer->y);
			break;
		default: 
			log_event(t->id, RESCUER_STATUS, "⬅ %s [%d] : 📍 [%d, %d] ... [%d, %d] 🏠- il rescuer  torna alla base", t->rescuer->rescuer_type_name, t->id, t->x, t-> y, t->rescuer->x, t->rescuer->y);
	}
	t->status = RETURNING_TO_BASE; 				// cambio lo stato del twin
	t->time_to_manage = INVALID_TIME;
	t->time_left_before_leaving = INVALID_TIME;
	
	change_rescuer_digital_twin_destination(t, t->rescuer->x, t->rescuer->y);
}

void change_rescuer_digital_twin_destination_blocked(rescuer_digital_twin_t *t, int new_x, int new_y){
	if(!t || (t->x == new_x && t->y == new_y)) return; 		// se non è cambiata la destinazione non faccio nulla
	change_bresenham_trajectory(t->trajectory, t->x, t->y, new_x, new_y);
	t->is_travelling = true;
	t->has_arrived = false;
	t->time_left_before_leaving = INVALID_TIME;
}

void update_active_emergencies_states_logging() {
	for (int i = 0; i < WORKER_THREADS_NUMBER; i++){
		update_active_emergency_state_logging_blocking(ctx->active_emergencies->array[i]);
	}
}

void update_active_emergency_state_logging_blocking(emergency_t *e) {
	mtx_lock(&e->mutex);
	if (atomic_load(&e->has_been_paused) <= 0) {
		e->status = PAUSED;
		cnd_signal(&e->cond);
		log_event(e->id, EMERGENCY_STATUS, "PAUSA DI (%d, %d) %s", e->x, e->y, e->type->emergency_desc);
	} else if (atomic_load(&e->rescuers_not_done_yet) <= 0) {
		e->status = COMPLETED;
		cnd_signal(&e->cond);
		log_event(e->id, EMERGENCY_STATUS, "EMERGENZA COMPLETATA (%d, %d) %s", e->x, e->y, e->type->emergency_desc);
	} else if (atomic_load(&e->rescuers_not_arrived_yet) <= 0) {
		e->status = IN_PROGRESS;
		log_event(e->id, EMERGENCY_STATUS, "%d rescuers in (%d, %d) per %s arrivati, al lavoro!", e->rescuer_count, e->x, e->y, e->type->emergency_desc);
	}
	mtx_unlock(&e->mutex);
}

void update_waiting_emergency_state(emergency_t *e) {
	if (!atomic_load(&e->promotion_timer) == NO_PROMOTION) {
		atomic_fetch_sub(&e->promotion_timer, 1);
	}
	if (!atomic_load(&e->timeout_timer) == NO_TIMEOUT) {
		atomic_fetch_sub(&e->timeout_timer, 1) <= 0;
	}
}

void update_waiting_emergencies_states() {
	pq_map(ctx->emergency_queue, update_waiting_emergency_state);
}

bool emergency_has_expired(emergency_t *e) {
	int time_left = atomic_load(&e->timeout_timer);
	if (time_left == NO_TIMEOUT) return false;
	if (time_left > 0) return false;
	return true;
}


void remove_expired_waiting_emergencies_logging() {
	emergency_t *e;
	while (e = pq_extract_first(ctx->emergency_queue, emergency_has_expired)){
		e->status = TIMEOUT;
		log_event(e->id, EMERGENCY_STATUS, "(%d, %d) %s è stata messa in TIMEOUT perchè in attesa da troppo tempo!", e->x, e->y, e->type->emergency_desc);
		pq_push(ctx->canceled_emergencies, e, e->priority);
	}
	// Siamo usciti dal while, quindi non ci sono più emergenze scadute!
	return;
}

bool emergency_is_to_promote(emergency_t *e){
	int time_left = atomic_load(&e->promotion_timer);
	if (time_left == NO_PROMOTION) return false;
	if (time_left > 0) return false;
	return true;
}

void promote_needing_emergencies(){
	emergency_t *e;
	while (e = pq_extract_first(ctx->emergency_queue, emergency_is_to_promote)){
		log_event(e->id, EMERGENCY_STATUS, "(%d, %d) %s è stata promossa a priorità superiore!", e->x, e->y, e->type->emergency_desc);
		e->priority = get_next_priority(e->priority);
		pq_push(ctx->canceled_emergencies, e, e->priority);
	}
	// Siamo usciti dal while, quindi non ci sono più emergenze da promuovere
	return;
}
