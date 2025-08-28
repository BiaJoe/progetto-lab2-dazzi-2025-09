#include "server.h"

// ----------- funzioni per il thread updater -----------

// funzione che gira ogni tick del clock
// fa l'update di tutte le strutture del server
// blocca tutti i mutex principali 
// per non fare inserimenti o rimozioni dalle queue e le liste durante l'update
// blocca i singoli nodi quando li sta usando
int thread_updater(void *arg){
	server_context_t *ctx = arg;

	while(!ctx->server_must_stop){		
		lock_server_clock(ctx); 																					
		while(!server_is_ticking(ctx)) 
			wait_for_a_tick(ctx); 		// attendo che il server ticki				
		untick(ctx); 					// il server ha tickato, lo sblocco		
		unlock_server_clock(ctx); 																				
		
		log_event(AUTOMATIC_LOG_ID, SERVER_UPDATE, "🔄  aggiornamento #%d del server iniziato...", ctx->clock->tick_count_since_start);

		// aggiorno lo stato del server: posizioni dei gemelli, stato delle emergenze etc...
										
		log_event(AUTOMATIC_LOG_ID, SERVER_UPDATE, "👍  aggiornamento #%d del server finito!", ctx->clock->tick_count_since_start);
	}
	return 0;
}

void update_rescuers_states_and_positions_on_the_map_logging(server_context_t *ctx){
	int amount = ctx->rescuers->count;
	for(int i = 0; i < amount; i++){													// aggiorno le posizioni dei gemelli rescuers
		rescuer_type_t *r = ctx->rescuers->types[i];
		if(r == NULL) continue; 																// se il rescuer type è NULL non faccio nulla (precauzione)
		for(int j = 0; j < r->amount; j++){
			rescuer_digital_twin_t *dt = r->twins[j];
			log_event(dt->id, DEBUG, "%s [%d] stato = %d", dt->rescuer->rescuer_type_name, dt->id, dt->status);
			update_rescuer_digital_twin_state_and_position_logging(dt, MIN_X_COORDINATE_ABSOLUTE_VALUE, MIN_Y_COORDINATE_ABSOLUTE_VALUE, ctx->enviroment->height, ctx->enviroment->width);
		}
	}
}

bool update_rescuer_digital_twin_state_and_position_logging(rescuer_digital_twin_t *t, int minx, int miny, int height, int width){
	if (!t || t->status == IDLE) 		// se non deve muoversi non faccio nulla
		return false; 										// la posizione non va aggiornata
	
	if(t->status == ON_SCENE){			// se sta gestendo un'emergenza
		if(--(t->time_left_before_leaving) > 0)
			return false;									// deve ancora stare sulla scena dell'emergenza
		else													// ha finito: può tornare alla base!
			send_rescuer_digital_twin_back_to_base_logging(t);
	}

	t->is_travelling = true; // ci accede il gemello vede che sta viaggiando
	
	int xA = t->x;
	int yA = t->y;
	int cells_to_walk_on_the_X_axis = 0;
	int cells_to_walk_on_the_Y_axis = 0;
	
	int we_have_arrived = compute_bresenham_step(
		t->x,
		t->y,
		t->trajectory,
		t->rescuer->speed,
		&cells_to_walk_on_the_X_axis,
		&cells_to_walk_on_the_Y_axis
	);

	t->x += cells_to_walk_on_the_X_axis;		// faccio il passo
	t->y += cells_to_walk_on_the_Y_axis;

	if (
		t->x < minx || t->x > width ||
		t->y < miny || t->y > height 
	) log_fatal_error("srescuer uscito dalla mappa");

	if (!we_have_arrived){
		log_event(t->id, RESCUER_TRAVELLING_STATUS, "📍 %s [%d] : [%d, %d] -> [%d, %d] il rescuer si è spostato", t->rescuer->rescuer_type_name, t->id, xA, yA, t->x, t->y);
		return true; 													// ho aggiornato la posizione
	}

	t->is_travelling = false; 									// siamo arrivati! non stiamo più viaggiando	
	t->has_arrived = true;	

	switch (t->status) {
		case EN_ROUTE_TO_SCENE:
			t->time_left_before_leaving = t->time_to_manage;
			t->status = ON_SCENE;
			log_event(t->id, RESCUER_STATUS, "🚨 %s [%d] -> [%d, %d] il rescuer è arrivato alla scena dell'emergenza  !!!!", t->rescuer->rescuer_type_name, t->id, t->x, t->x);
			return true;		
		case RETURNING_TO_BASE:
			t->status = IDLE;
			log_event(t->id, RESCUER_STATUS, "⛑️ %s [%d] -> [%d, %d] il rescuer è tornato sano e salvo alla base  :)", t->rescuer->rescuer_type_name, t->id, t->x, t->x);
			return true;
		default: log_fatal_error("spostamento rescuer dt");
	}	
	return false; // non si arriva qui, si fallisce prima
}

void send_rescuer_digital_twin_back_to_base_logging(rescuer_digital_twin_t *t){			
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
	
	change_rescuer_digital_twin_destination(t, t->rescuer->x, t->rescuer->y);
}

void change_rescuer_digital_twin_destination(rescuer_digital_twin_t *t, int new_x, int new_y){
	if(!t || (t->x == new_x && t->y == new_y)) return; 		// se non è cambiata la destinazione non faccio nulla
	change_bresenham_trajectory(t->trajectory, t->x, t->y, new_x, new_y);
	t->is_travelling = true;
	t->has_arrived = false;
	t->time_left_before_leaving = INVALID_TIME;
}
