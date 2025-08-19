#include "server_emergency_handler.h"

// ----------- funzioni per il thread worker -----------


// aspetta che ci sia almeno un nodo da processare
// prende il nodo a priorità più alta più vecchio (hottest)
// esegue i processi necessari per la sua elaborazione
// ad ogni "step" controlla che l'emergenza non sia stata cancellata o messa in pausa
// se lo è stata agisce di conseguenza e passa alla prossima, altrimenti va avanti su quella
int server_emergency_handler(void *arg){
	server_context_t *ctx = arg;
	emergency_node_t *n = NULL;
	emergency_queue_t *waiting_queue = ctx->waiting_queue; 	// estraggo la coda delle emergenze in attesa
	emergency_queue_t *working_queue = ctx->working_queue; 	// estraggo la coda delle emergenze in che sto processando

	while(!ctx->server_must_stop){		
		// attendo che ci sia qualcosa da processare	
		lock_queue(waiting_queue); 			
		while(waiting_queue->is_empty) 	
			cnd_wait(&waiting_queue->not_empty, &waiting_queue->mutex);
		n = dequeue_emergency_node(waiting_queue);
		unlock_queue(waiting_queue);

		// c'è almeno un nodo da processare: metto il nodo più caldo nella coda di lavoro	
		lock_queue(working_queue); 									
		enqueue_emergency_node(working_queue, n); 	
		unlock_queue(working_queue); 	
		 
		log_event(n->emergency->id, EMERGENCY_STATUS, "inizio a lavorare sull'emergenza %d: %s (%d) [%d, %d] ", n->emergency->id, n->emergency->type->emergency_desc, n->emergency->priority, n->emergency->x, n->emergency->y);

		lock_node(n);
		if(n->emergency->status == WAITING) 										// se l'emergenza non è mai stata assegnata prima
			n->emergency->time_since_it_was_assigned = 0;					// inizia a scorrere il timer dell'assegnamento
		n->emergency->status = ASSIGNED; 												// cambio lo stato dell'emergenza a ASSIGNED
	 	
		// funzioni che automaticamente aspettano che i rescuer vengano trovati, arrivino o finiscano il lavoro
		// e ritornano NO (falso) se per qualche motovo si deve passare ad un nodo successivo
		if (!handle_search_for_rescuers(ctx, n)) 						continue; 
		if (!handle_waiting_for_rescuers_to_arrive(ctx, n)) continue;
		if (!handle_emergency_processing(ctx, n)) 					continue;

		// tutti gli altri casi sono stati gestiti: l'emergenza è completata!

		n->emergency->status = COMPLETED;	
		log_event(n->emergency->id, EMERGENCY_STATUS, "L'emergenza %d: %s (%d) [%d, %d] è stata completata!!! che bello :D. tutti e %d i suoi rescuers sono stati rimandati alla base", n->emergency->id, n->emergency->type->emergency_desc, n->emergency->priority, n->emergency->x, n->emergency->y, n->emergency->rescuer_count);
		move_working_node_to_completed_blocking(ctx, n);						

		unlock_node(n);
	}		
	return 0;
}

int handle_search_for_rescuers(server_context_t *ctx, emergency_node_t *n){
	n->emergency->status = ASKING_FOR_RESCUERS;
	log_event(n->emergency->id, EMERGENCY_STATUS, "🔎 %s [%d] (%d) [%d,%d] inizia a cercare rescuers",n->emergency->rescuer_count, n->emergency->id, n->emergency->type->emergency_desc, n->emergency->priority, n->emergency->x, n->emergency->y);
	while (!n->rescuers_found && n->emergency->status != CANCELED) {		// cerco i rescuers ogni tot secondi finchè o li ho trovati o l'emergenza è stata cancellata
		lock_rescuer_types(ctx); 																						
		find_and_send_nearest_rescuers(n, RESCUER_SEARCHING_STEAL_MODE); // cerco rescuer, posso anche rubarli da emergenze meno importanti se ce ne sono
		unlock_rescuer_types(ctx); 		
		if (n->rescuers_found || n->emergency->status == CANCELED) 
			break;
		log_event(n->emergency->id, EMERGENCY_STATUS, "❎ %s [%d] (%d) [%d,%d] non ha trovato rescuers disponibili, riprova.",  n->emergency->type->emergency_desc, n->emergency->id, n->emergency->priority, n->emergency->x, n->emergency->y);
		struct timespec ts;
		timespec_get(&ts, TIME_UTC);
		ts.tv_sec += TIME_INTERVAL_BETWEEN_RESCUERS_SEARCH_ATTEMPTS_SECONDS;
		cnd_timedwait(&n->waiting, &n->mutex, &ts);			// aspetto qualche secondo e cerco di nuovo i rescuers
	}
	if (n->rescuers_found)
		return YES;
	if (n->emergency->status == CANCELED)
		cancel_and_unlock_working_node_blocking(ctx, n);
	return NO;
}

int handle_waiting_for_rescuers_to_arrive(server_context_t *ctx, emergency_node_t *n){
	n->emergency->status = WAITING_FOR_RESCUERS;			// tutto ok: i rescuers stanno arrivando
	log_event(n->emergency->id, EMERGENCY_STATUS, "✅ %s [%d] (%d) [%d, %d] ha i rescuer necessari in arrivo, si aspetta che arrivino", n->emergency->type->emergency_desc, n->emergency->id, n->emergency->priority, n->emergency->x, n->emergency->y);
	while (!n->rescuers_have_arrived && n->emergency->status != PAUSED && n->emergency->status != CANCELED)
		cnd_wait(&n->waiting, &n->mutex);								// aspetto finchè o arrivano i rescuers o l'emergenza è messa in pausa o cancellata
	if (n->rescuers_have_arrived)
		return YES;
	if (n->emergency->status == PAUSED)
		pause_and_unlock_working_node_blocking(ctx, n);
	if (n->emergency->status == CANCELED)
		cancel_and_unlock_working_node_blocking(ctx, n);
	return NO;
}

int handle_emergency_processing(server_context_t *ctx, emergency_node_t *n){
	n->emergency->status = IN_PROGRESS;								// i rescuer iniziano a lavorare, il thread_updater controllerà se l'emergenza è finita
	log_event(n->emergency->id, EMERGENCY_STATUS, "✳️ L'emergenza %d: %s (%d) [%d, %d] ha tutti i rescuers sul posto! Inizia ad essere processata", n->emergency->id, n->emergency->type->emergency_desc, n->emergency->priority, n->emergency->x, n->emergency->y);
	while (n->emergency->status != COMPLETED && n->emergency->status != PAUSED && n->emergency->status != CANCELED)
		cnd_wait(&n->waiting, &n->mutex);								// aspetto finchè o arrivano i rescuers o l'emergenza è messa in pausa o cancellata
	if(n->emergency->status == COMPLETED)
		return YES;
	if (n->emergency->status == PAUSED)
		pause_and_unlock_working_node_blocking(ctx, n);
	if (n->emergency->status == CANCELED)
		cancel_and_unlock_working_node_blocking(ctx, n);
	return NO;
}

void cancel_and_unlock_working_node_blocking(server_context_t *ctx, emergency_node_t *n){
	lock_queue(ctx->working_queue);													
	lock_list(ctx->canceled_emergencies);
	change_emergency_node_list_append(n, ctx->canceled_emergencies);
	unlock_list(ctx->canceled_emergencies);
	unlock_queue(ctx->working_queue);
	unlock_node(n);
}

void pause_and_unlock_working_node_blocking(server_context_t *ctx, emergency_node_t *n){
	lock_queue(ctx->waiting_queue);
	lock_queue(ctx->working_queue);													
	// lock_list(ctx->working_queue->lists[n->emergency->priority]);
	change_emergency_node_list_push(n, ctx->canceled_emergencies);
	// unlock_list(ctx->working_queue->lists[n->emergency->priority]);
	unlock_queue(ctx->working_queue);
	unlock_queue(ctx->waiting_queue);
	unlock_node(n);
}

void move_working_node_to_completed_blocking(server_context_t *ctx, emergency_node_t *n){
	lock_queue(ctx->working_queue);													
	lock_list(ctx->completed_emergencies);
	change_emergency_node_list_append(n, ctx->completed_emergencies);
	unlock_list(ctx->completed_emergencies);
	unlock_queue(ctx->working_queue);
}

// funzioni usate per trovare e inviare rescuer verso l'emergenza

void send_rescuer_digital_twin_to_scene_logging(rescuer_digital_twin_t *t, emergency_node_t *n){
	t->emergency_node = n; // il rescuer prende l'emergenza verso cui deve andare
	emergency_t *e = n->emergency;
	rescuer_request_t *request = get_rescuer_request_by_name(t->rescuer->rescuer_type_name, e->type->rescuers);
	t->time_to_manage = request->time_to_manage;	// ottengo quanto devo aspettare sul posto dell'emergenza verso cui sto andando
	int x = e->x;
	int y = e->y;

	switch (t->status) {
		case IDLE: 
			log_event(t->id, RESCUER_STATUS, "➡️ %s [%d]: 🏠 [%d,%d] --> [%d,%d] ⚠️ %s - il rescuer parte dalla base verso la scena dell'emergenza", t->rescuer->rescuer_type_name, t->id, t->x, t->y, x, y, e->type->emergency_desc);
			break;
		case EN_ROUTE_TO_SCENE:
			log_event(t->id, RESCUER_STATUS, "➡️ %s [%d]: 🚀 [%d,%d] --> [%d,%d] ⚠️ %s - il rescuer cambia destinazione verso la scena di un'altra emergenza", t->rescuer->rescuer_type_name, t->id, t->x, t->y, x, y, e->type->emergency_desc);
			break; 
		case ON_SCENE:
			log_event(t->id, RESCUER_STATUS, "➡️ %s [%d]: ⚠️ [%d,%d] --> [%d,%d] ⚠️ %s - il rescuer va via dall'emergenza attuale per gestirne un'altra", t->rescuer->rescuer_type_name, t->id, t->x, t->y, x, y, e->type->emergency_desc);
			break; 
		case RETURNING_TO_BASE: // caso contemplato in questa funzione ma non verificato perchè le specifiche del progetto non lo contemplano. L'ho messo per completezza e per facilitare un eventuale cambio futuro delle regole
			log_event(t->id, RESCUER_STATUS, "➡️ %s [%d]: 🏡 [%d,%d] <-> [%d,%d] ⚠️ %s - il rescuer sta tornando alla base ma cambia destinazione per andare a gestire", t->rescuer->rescuer_type_name, t->id, t->x, t->y, x, y, e->type->emergency_desc);
			break;
		default:
			log_event(t->id, FATAL_ERROR, "%s [%d] ??? stato del rescuer non riconosciuto, impossibile mandarlo alla scena di un'emergenza", t->rescuer->rescuer_type_name, t->id);
			exit(EXIT_FAILURE);
	}
	t->status = EN_ROUTE_TO_SCENE;
	change_rescuer_digital_twin_destination(t, x, y);
}	


int is_rescuer_digital_twin_available(rescuer_digital_twin_t *dt){			
	return (dt->status == IDLE) ? YES : NO;	// l'ho incapsulato in una funzione perchè così è più semplice cambiare i requisiti, ad esempio potremmo voler chiamare i rescuer anche quando tornano alla base 																																					
}

int is_rescuer_digital_twin_already_chosen(rescuer_digital_twin_t *dt, rescuer_digital_twin_t **chosen, int count) {
	for (int i = 0; i < count; i++) {
		if (chosen[i] == dt) return YES;
	}
	return NO;
}

rescuer_digital_twin_t *find_nearest_available_rescuer_digital_twin(rescuer_type_t *r, emergency_node_t *n, rescuer_digital_twin_t **chosen, int count){
	int min_distance = MAX(MAX_X_COORDINATE_ABSOLUTE_VALUE, MAX_Y_COORDINATE_ABSOLUTE_VALUE); 	// inizializzo la distanza minima a un valore molto grande, nessun valore sarà mai così grande
	rescuer_digital_twin_t *nearest_dt = NULL; 																									// inizializzo il gemello digitale più vicino a NULL
	for (int i = 0; i < r->amount; i++) { 																											// scorro tutti i gemelli digitali del rescuer type
		rescuer_digital_twin_t *dt = get_rescuer_digital_twin_by_index(r, i); 										
		if (!is_rescuer_digital_twin_available(dt)) 
			continue; 			
		if (is_rescuer_digital_twin_already_chosen(dt, chosen, count)) 
			continue;																									
		int d = MANHATTAN(dt->x, dt->y, n->emergency->x, n->emergency->y);
		if (d < min_distance) { 																																		
			min_distance = d; 																		
			nearest_dt = dt; 																																				
		}
	}
	return nearest_dt;																				
}

int is_rescuer_digital_twin_stealable(rescuer_digital_twin_t *dt, emergency_t *stealer_emergency){
	if (dt->emergency_node == NULL) return NO;

	lock_node(dt->emergency_node);
	emergency_t *current = dt->emergency_node->emergency;

	// evito di rubare se già assegnato alla stessa emergenza
	int result = (
		current->id != stealer_emergency->id &&
	  (dt->status == EN_ROUTE_TO_SCENE || dt->status == ON_SCENE) &&
	  current->priority < stealer_emergency->priority
	) ? YES : NO;

	unlock_node(dt->emergency_node);
	return result;
}

int rescuer_digital_twin_must_be_stolen(rescuer_digital_twin_t *dt){
	return (
		(dt->status == EN_ROUTE_TO_SCENE || dt->status == ON_SCENE) &&
		(dt->emergency_node->emergency->status == WAITING_FOR_RESCUERS || dt->emergency_node->emergency->status == IN_PROGRESS)
	) ? YES : NO;
}

rescuer_digital_twin_t *try_to_find_nearest_rescuer_from_less_important_emergency(rescuer_type_t *r, emergency_node_t *n, rescuer_digital_twin_t **chosen, int count){
	int min_distance = MAX(MAX_X_COORDINATE_ABSOLUTE_VALUE, MAX_Y_COORDINATE_ABSOLUTE_VALUE); 	// inizializzo la distanza minima a un valore molto grande, nessun valore sarà mai così grande
	int d = min_distance; 																																			
	rescuer_digital_twin_t *nearest_dt = NULL; 		
	for (int i = 0; i < r->amount; i++) { 																											// scorro tutti i gemelli digitali del rescuer type
		rescuer_digital_twin_t *dt = get_rescuer_digital_twin_by_index(r, i); 										
		if (!is_rescuer_digital_twin_stealable(dt, n->emergency)) 
			continue; 								
		if(is_rescuer_digital_twin_already_chosen(dt, chosen, count)) 
			continue;																					
		d = MANHATTAN(dt->x, dt->y, n->emergency->x, n->emergency->y);
		if (d < min_distance) { 																																		
			min_distance = d; 																		
			nearest_dt = dt; 																																				
		}
	}
	return nearest_dt;		
}

// cambia lo stato dell'emergenza a paused
// agisce su un nodo che può essere "derubato"
// il nodo è nel suo thread che sta aspettando che i suoi gemelli arrivino al punto dell'emergenza
// oppure aspetta che abbiano finito di stare lí
void pause_emergency_blocking_signaling_logging(emergency_node_t *n){ 
	lock_node(n);
	emergency_t *e = n->emergency;
	if (e->status == PAUSED){					// se l'emergenza è già in pausa non faccio niente (misura di sicurezza per non mettere in pausa un'emergenza e loggare l'evento più di una volta)
		unlock_node(n);
		return;
	}
	e->status = PAUSED;
	n->rescuers_are_arriving = NO;
	n->rescuers_have_arrived = NO;
	n->rescuers_found = NO;
	n->time_estimated_for_rescuers_to_arrive = INVALID_TIME;
	cnd_signal(&n->waiting);
	unlock_node(n);
	log_event(e->id, EMERGENCY_STATUS, "Emergenza %s [%d] (%d) [%d,%d] messa in pausa", e->type->emergency_desc, e->id, e->priority, e->x, e->y);
}

// cerca tra i rescuer disponibili e li assegna se li trova tutti
// nella fase di ricerca non apporta nessuna modifica ai rescuer o ai loro nodi
// se ha trovato abbastanza rescuer allora passa alla fase di invio
// se è in modalità STEAL può mettere in pausa emergenze e "rubare" i loro rescuer
int find_and_send_nearest_rescuers(emergency_node_t *n, char mode){	
	int we_can_steal;
	switch (mode) {
		case RESCUER_SEARCHING_FAIR_MODE: 	we_can_steal = NO; 	break;
		case RESCUER_SEARCHING_STEAL_MODE: we_can_steal = YES; break;
		default: we_can_steal = NO;
	}

	rescuer_digital_twin_t *rescuers[n->emergency->rescuer_count]; 												// creo un array temporaneo di rescuers della dimensione del numero di rescuers richiesti dall'emergenza
	int rescuer_index = 0; 																																	
	int rescuer_types_amount = n->emergency->type->rescuers_req_number;
	int max_time_to_arrive = 0;
	int time_to_arrive = INVALID_TIME; 												// inizializzo il tempo massimo di arrivo a un valore invalido
	
	// parte della funzione che cerca i rescuer (e ritorna se non li trova)
	for(int i = 0; i < rescuer_types_amount; i++){																				// scorro i rescuer types richiesti
		int rescuer_dt_amount = n->emergency->type->rescuers[i]->required_count; 	
		rescuer_type_t *rt = n->emergency->type->rescuers[i]->type; 												// prendo il rescuer type i-esimo
		for(int j = 0; j < rescuer_dt_amount; j++){																					// per ognuno, devo trovare i gemelli richiesti
			rescuer_digital_twin_t *dt = find_nearest_available_rescuer_digital_twin(rt, n, rescuers, rescuer_index);
			// un rescuer libero non l'ho trovato, magari posso rubarne uno se sono nella modalitàRESCUER_SEARCHING_STEAL_MODE 
			if(we_can_steal && dt == NULL)
				dt = try_to_find_nearest_rescuer_from_less_important_emergency(rt, n, rescuers, rescuer_index);
			if (dt == NULL) {
				n->rescuers_are_arriving = NO;
				n->rescuers_found = NO;
				return NO;													// se non lo trovo è inutile andare avanti
			}																			
			rescuers[rescuer_index++] = dt; 			// lo aggiungo all'array temporaneo di rescuers
		}
	}

	// parte della funzione che assegna e invia i rescuer (se prima li ha trovati)
	for(int i = 0; i < rescuer_index; i++){ 	
		rescuer_digital_twin_t *dt = rescuers[i];
		if (we_can_steal && rescuer_digital_twin_must_be_stolen(dt))
			pause_emergency_blocking_signaling_logging(dt->emergency_node);
		int s = MANHATTAN(dt->x, dt->y, n->emergency->x, n->emergency->y);			
		int v = dt->rescuer->speed; 			
		time_to_arrive = ABS(s / v) + 1;
		if (time_to_arrive > max_time_to_arrive) 		// trovo il tempo che ci mette ad arrivare il rescuer più lontano/lento
			max_time_to_arrive = time_to_arrive; 
		n->emergency->rescuer_twins[i] = dt; 															// aggiungo il gemello digitale all'emergency perchè ora sono sicuro che è valido
		send_rescuer_digital_twin_to_scene_logging(dt, n); 								// mando i gemelli sulla scena, ritorno il tempo stimato perchè tutti arrivino
	}
	
	n->time_estimated_for_rescuers_to_arrive = max_time_to_arrive; 			// aggiorno il tempo stimato per l'arrivo dei rescuers
	n->rescuers_found= YES;
	n->rescuers_are_arriving = YES; 																		// segno che i rescuers stanno arrivando
	return YES; 																												// ritorno il tempo stimato per l'arrivo dell'ultimo rescuer. Serve per capire se devo o non devo mettere l'emergenza in timeout
}


