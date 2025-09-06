#include "server.h"

extern server_context_t *ctx;

// gira ad ogni tick
// - aggiorna le posizioni dei rescuers
// - fa camminare i rescuers di "un passo" verso i loro obiettivi
// - aggiorna lo stato delle emergenze:
// - timeout di quelle scadute
// - promozione di quelle in attesa da troppo 0->1
// - risveglio dei thread che lavorano su quelle attive se è successo qualcosa
int thread_updater(void *arg){
	log_register_this_thread("UPDATER");
	while(!atomic_load(&ctx->server_must_stop)){		
		lock_server_clock();
        while (!server_is_ticking() && !atomic_load(&ctx->server_must_stop))
            wait_for_a_tick();  
        if (atomic_load(&ctx->server_must_stop)) { 
            unlock_server_clock();
            break;
        }
        untick();
        unlock_server_clock();

		int cycle = get_current_tick_count();
		log_event(AUTOMATIC_LOG_ID, SERVER_UPDATE, "Aggiornamento #%d del server", cycle);
		
		lock_system();
		update_rescuers_states_logging();
		update_active_emergencies_states_logging();
		json_visualizer_append_tick(cycle);
		unlock_system();

		update_waiting_emergencies_states();
		remove_expired_waiting_emergencies_logging();
		promote_needing_waiting_emergencies();
	
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
			update_rescuer_digital_twin_state_logging_blocked(dt, MIN_X_COORDINATE_ABSOLUTE_VALUE, MIN_Y_COORDINATE_ABSOLUTE_VALUE, ctx->enviroment->height, ctx->enviroment->width);
		}
	}
}

bool update_rescuer_digital_twin_state_logging_blocked(rescuer_digital_twin_t *t, int minx, int miny, int height, int width){
	if (!t) return false; 			
	if (t->status == IDLE) return false;

	// se deve lasciare la scena cambio la sua destinazione, ma non esco perchè la posizione va ancora aggiornata
	if(t->status == ON_SCENE) {
		if (--(t->time_left_before_leaving) > 0) return false;
		// tolgo il rescuer da quella emergenza
		LOG_RESCUER_STATUS_DEBUG(t, "ha finito! torna alla base");
		t->emergency->rescuers_not_done_yet--;
		remove_twin_from_its_emergency(t);
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

	if (t->x < minx || t->x >= width || t->y < miny || t->y >= height) {
		log_error_and_exit(request_shutdown_from_thread, "Rescuer %s #%d uscito dalla mappa (%d, %d)", t->rescuer->rescuer_type_name, t->id, t->x, t->y);
	}

	if (!we_have_arrived){
		LOG_RESCUER_MOVED(t, xA, yA);
		return true; // ho aggiornato la posizione
	}

	// siamo arrivati! non stiamo più viaggiando	
	t->is_travelling = false;
	switch (t->status) {
		case EN_ROUTE_TO_SCENE:
			t->time_left_before_leaving = t->time_to_manage;
			t->status = ON_SCENE;
			t->emergency->rescuers_not_arrived_yet--;	// un rescuer in più è arrivato!
			LOG_RESCUER_ARRIVED(t, "il rescuer è arrivato alla scena dell'emergenza !!!!");
			return true;		
		case RETURNING_TO_BASE:
			t->status = IDLE;
			LOG_RESCUER_ARRIVED(t, "il rescuer è tornato sano e salvo alla base :)");
			return true;
		default: 
			log_error_and_exit(request_shutdown_from_thread, "spostamento rescuer dt");
	}	
	return false; // non si arriva qui, si fallisce o riesce prima
}

void send_rescuer_digital_twin_back_to_base_logging_blocked(rescuer_digital_twin_t *t){			
	switch (t->status) {
		case IDLE: 				return;									// se è già alla base non faccio nulla
		case RETURNING_TO_BASE: return;
		case ON_SCENE: 			LOG_RESCUER_SENT(t, t->rescuer->x, t->rescuer->y, "inizia il viaggio: emergenza -> base"); break;
		case EN_ROUTE_TO_SCENE: LOG_RESCUER_SENT(t, t->rescuer->x, t->rescuer->y, "cambio di rotta: verso emergenza -> verso base"); break;
		default: 				LOG_RESCUER_SENT(t, t->rescuer->x, t->rescuer->y, "il rescuer torna alla abase"); break;
	}

	t->status = RETURNING_TO_BASE; 				// cambio lo stato del twin
	t->time_to_manage = INVALID_TIME;
	t->time_left_before_leaving = INVALID_TIME;
	
	change_rescuer_digital_twin_destination_blocked(t, t->rescuer->x, t->rescuer->y);
}

void update_active_emergencies_states_logging() {
	for (int i = 0; i < WORKER_THREADS_NUMBER; i++) {
		update_active_emergency_state_logging_blocked(ctx->active_emergencies->array[i]);
	}
}

void promote_emergency_logging_blocked(emergency_t *e) {
	e->priority = get_next_priority(e->priority);
	e->timeout_timer = priority_to_timeout_timer(e->priority);
	e->promotion_timer = priority_to_promotion_timer(e->priority);	
	e->how_many_times_was_promoted++;	
	LOG_EMERGENCY_STATUS(e, "PROMOSSA");
}

void update_active_emergency_state_logging_blocked(emergency_t *e) {	
	if (!e) return;
	if (e->promotion_timer != NO_PROMOTION) {
		if (e->promotion_timer-- <= 0) promote_emergency_logging_blocked(e);
	} else if (e->status == PAUSED) {
		e->timeout_timer--;
	} else if (e->rescuers_not_done_yet <= 0) {
		e->status = COMPLETED;
		cnd_signal(&e->cond);
	} else if (e->rescuers_not_arrived_yet <= 0 && e->status == ASSIGNED) {
		e->status = IN_PROGRESS;
		LOG_EMERGENCY_STATUS(e, "IN PROGRESS");
	}
}



void update_waiting_emergency_state(void *arg) {
	emergency_t *e = (emergency_t *)arg;
	if (e->promotion_timer != NO_PROMOTION) e->promotion_timer--;
	if (e->timeout_timer != NO_TIMEOUT) e->timeout_timer--;
}

void update_waiting_emergencies_states() {
	pq_map(ctx->emergency_queue, update_waiting_emergency_state);
}

bool emergency_has_expired(void *arg) {
	emergency_t *e = (emergency_t *)arg;
	if (e->timeout_timer == NO_TIMEOUT) return false;
	if (e->timeout_timer > 0) return false;
	return true;
}

void remove_expired_waiting_emergencies_logging() {
	emergency_t *e;
	while ((e = pq_extract_first(ctx->emergency_queue, emergency_has_expired))){
		e->status = TIMEOUT;
		LOG_EMERGENCY_STATUS_SHORT(e, "TIMEOUT prima di essere processata");
		pq_push(ctx->canceled_emergencies, e, priority_to_level(e->priority));
	}
	// Siamo usciti dal while, quindi non ci sono più emergenze scadute!
	return;
}

bool emergency_is_to_promote(void *arg){
	emergency_t *e = (emergency_t *)arg;
	if (e->promotion_timer == NO_PROMOTION) return false;
	if (e->promotion_timer > 0) return false;
	return true;
}

void promote_needing_waiting_emergencies(){
	emergency_t *e;
	while ((e = pq_extract_first(ctx->emergency_queue, emergency_is_to_promote))){
		promote_emergency_logging_blocked(e);
		pq_push(ctx->emergency_queue, e, priority_to_level(e->priority));
	}
	// Siamo usciti dal while, quindi non ci sono più emergenze da promuovere
	return;
}
