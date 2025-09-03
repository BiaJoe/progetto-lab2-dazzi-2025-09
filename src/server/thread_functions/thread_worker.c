#include "server.h"

extern server_context_t *ctx;

static void open_slot(int i, emergency_t *e) {
	lock_emergencies();
	ctx->active_emergencies->array[i] = e;
	e->status = ASSIGNED;
	unlock_emergencies();
}

static void close_slot_blocked(int i, emergency_t *e) {
	ctx->active_emergencies->array[i] = NULL;
}

static void close_slot(int i) {
	lock_emergencies();
	ctx->active_emergencies->array[i] = NULL;
	unlock_emergencies();
}

static void change_status_store_close_logging_blocking(int i, emergency_t *e, emergency_status_t news, pq_t *queue) {
	if (!e || !queue) return;
	lock_emergencies();
	e->status = news;
	switch (news) {
		case TIMEOUT: 	LOG_EMERGENCY_STATUS_SHORT(e, "TIMEOUT: non abbastanza risorse"); break;
		case CANCELED: 	LOG_EMERGENCY_STATUS_SHORT(e, "CANCELLATA"); break;
		case COMPLETED: LOG_EMERGENCY_STATUS_SHORT(e, "COMPLETATA!! :)"); break;
		default:		break;
	}
	pq_push(queue, e, priority_to_level(e->priority));
	close_slot_blocked(i, e);
	unlock_emergencies();
}

int thread_worker(void *arg){
	// l'indice personal del worker, è quello con cui accede alla sua casella di emergenza
    const int index = atomic_fetch_add(&ctx->active_emergencies->next_tw_index, 1);
    if (index >= WORKER_THREADS_NUMBER) {
        log_error_and_exit(close_server, "non abbastanza slot di emergenze per i thread workers");
    }

	char tname[MAX_THREAD_NAME_LENGTH];
    snprintf(tname, sizeof(tname), "WORKER #%d", index);
    log_register_this_thread(tname);  

	log_event(index, DEBUG, "THREAD WORKER #%d CREATO", index);

	// variabili per dare nomi più semplici alle cose
	pq_t *queue = ctx->emergency_queue;

	// ciclo che estrae ed elabora un'emergenza alla volta
	while(!atomic_load(&ctx->server_must_stop)){	
		emergency_t *e = (emergency_t *)pq_pop(queue);
		if (!e) break;
		LOG_EMERGENCY_STATUS(e, "ASSEGNATA");
		open_slot(index, e);

		// ciclo che gira finchè l'emergenza non ha trovato i rescuers ed è stata completata o cancellata
		while(!atomic_load(&ctx->server_must_stop)){ 
			if (!find_rescuers_logging_blocking(e)) {
				change_status_store_close_logging_blocking(index, e, TIMEOUT, ctx->canceled_emergencies);
				break;
			}

			// i rescuer stanno arrivando, mi metto in attesa
			lock_emergencies();
			while(
				e->status != PAUSED && 
				e->status != COMPLETED && 
				!atomic_load(&ctx->server_must_stop)
			) { cnd_wait(&e->cond, &ctx->active_emergencies->mutex); }
			unlock_emergencies();
			
			
			if (atomic_load(&ctx->server_must_stop)) break;
			if (e->status == PAUSED) continue;				// riprendo la ricerca dei rescuers rubati
			if (e->status == COMPLETED) change_status_store_close_logging_blocking(index, e, e->status, ctx->completed_emergencies);
			if (e->status == CANCELED) 	change_status_store_close_logging_blocking(index, e, e->status, ctx->canceled_emergencies);
			
			break;
			// in base a che stato ha e prendiamo provvedimenti
		}
		if (atomic_load(&ctx->server_must_stop)) {
			change_status_store_close_logging_blocking(index, e, CANCELED, ctx->canceled_emergencies);
			close_slot(index);
			break;
		}
		close_slot(index);
	}		
	return 0;
}



static int estimated_arrival_time(emergency_t *e, rescuer_digital_twin_t *t) {
	int s = MANHATTAN(e->x, e->y, t->x, t->y);
	int v = t->rescuer->speed;
	return (s / v);
}

static bool is_preemptable_or_available(rescuer_digital_twin_t* t, short priority) {
	if(t->status == IDLE)
		return true;
	if(t->status == RETURNING_TO_BASE)
		return false;
	if(t->status == ON_SCENE || t->status == EN_ROUTE_TO_SCENE){
		if (t->emergency && (compare_priorities(t->emergency->priority, priority) >= 0))
			return false;
	}
	return true;
}

// risponde lo stato migliore
static inline int twin_state_rank(rescuer_status_t s) {
    switch (s) {
        case IDLE:               return 0; // migliore
        case EN_ROUTE_TO_SCENE:  return 1;
        case ON_SCENE:           return 2;
        default:                 return 4; // qualsiasi altro stato sconosciuto: peggiore
    }
}

// Ritorna:
//   > 0  se a è preferibile a b
//   < 0  se b è preferibile a a
//   = 0  se equivalenti
static int compare_twins(emergency_t *e, rescuer_digital_twin_t *a, rescuer_digital_twin_t *b) {
    int sa = twin_state_rank(a->status);
    int sb = twin_state_rank(b->status);
    if (sa != sb) return (sa < sb) ? +1 : -1;

    int ta = estimated_arrival_time(e, a);
    int tb = estimated_arrival_time(e, b);
    return tb - ta; // meglio chi arriva prima
}


static rescuer_digital_twin_t* find_best_twin_blocked(emergency_t *e, rescuer_type_t *r) {
	log_event(AUTOMATIC_LOG_ID, DEBUG, "sto cercando un twin");
	rescuer_digital_twin_t* best_so_far = NULL;
	for (int i = 0; i < r->amount; i++) {
		rescuer_digital_twin_t* t = r->twins[i];
		log_event(AUTOMATIC_LOG_ID, DEBUG, "Analizzo (%d, %d) v=%d %s %d",t->x, t->y, t->rescuer->speed, t->rescuer->rescuer_type_name, t->id);
		if (!t) continue;
		if (t->is_a_candidate) continue;
		if (!is_preemptable_or_available(t, e->priority)) continue;
		if (t->emergency && t->emergency->id == e->id) continue; 
		int eat = estimated_arrival_time(e, t);
    	log_event(AUTOMATIC_LOG_ID, DEBUG, "ETA: %d, TBT: %d", eat, e->timeout_timer);
		if (e->timeout_timer != NO_TIMEOUT && eat > e->timeout_timer) {
			continue;
		}
		if (best_so_far == NULL) {
			best_so_far = t;
			continue;
		}
		if (compare_twins(e, best_so_far, t) < 0)
			best_so_far = t;
	} 
	return best_so_far;
}

// ruba il twin dalla sua vecchia emergenza e la manda in pausa 
// sicuramente il rescuer si può preemptare, altrimenti non lo avremmo preso
// ma se è idle non faccio niente
static void preempt_if_needed_blocked(rescuer_digital_twin_t *t, emergency_t *newe){
	if(!t || t->status == IDLE || !t->emergency) return;
	emergency_t *old = t->emergency;

	// old adesso non ha più uno dei rescuers del tipo di t
	rescuer_request_t *r = get_rescuer_request_by_name(t->rescuer->rescuer_type_name, old->rescuers_missing);
	r->required_count++;

	if (t->status == ON_SCENE) {
		old->rescuers_not_arrived_yet++;
	}	

	remove_twin_from_its_emergency(t);
	
	// se non l'avevo già messa in pausa per un rescuer precedente, lo faccio
	if (old->status != PAUSED) {
        old->status = PAUSED;
        cnd_signal(&old->cond);
        log_event(old->id, EMERGENCY_STATUS, "(%d, %d) %s PAUSATA da (%d, %d) %s", old->x, old->y, old->type->emergency_desc, newe->x, newe->y, newe->type->emergency_desc);
    }
}


bool find_rescuers_logging_blocking(emergency_t *e) {
	lock_system();
	bool found = true;

	// calcolo di quanti rescuers ho bisogno e alloco un array di candidati
	int missing_count = 0;
	for (int i = 0; i < e->type->rescuers_req_number; i++) {
		missing_count += e->rescuers_missing[i]->required_count;
	}

	if (missing_count == 0) { 
		unlock_system(); 
		return true; 
	}

	int k = 0;
	rescuer_digital_twin_t *candidates[missing_count]; 
	// scorro i tipi di rescuers di cui l'emergenza ha bisogno
	for (int i = 0; i < e->type->rescuers_req_number; i++) {
		int count = e->rescuers_missing[i]->required_count;
		rescuer_type_t *r = e->rescuers_missing[i]->type;
		// cerco il numero di rescuers che mi servono, prendo ogni volta il migliore possibile
		for (int j = 0; j < count; j++) {
			rescuer_digital_twin_t *t = find_best_twin_blocked(e, r);
			if (t == NULL) { found = false; break; }
			t->is_a_candidate = true;
			candidates[k++] = t;
		}
		if(found == false)
			break;
	}

	for(int i = 0; i < k; i++) {
		candidates[i]->is_a_candidate = false;
	}

	if (!found) {
		unlock_system();
		return false;
	}
	
	// ho trovato i rescuer necessari
	// li assegno all'emergenza e li spedisco verso essa
	k = 0;
	for (int i = 0; i < e->rescuer_count; i++) {
		// se l'emergenza ha già dei rescuers che ci stanno lavorando non faccio niente
		if (e->rescuer_twins[i] != NULL) continue;
		rescuer_digital_twin_t *t = candidates[k++];
		preempt_if_needed_blocked(t, e);
		e->rescuer_twins[i] = t;
		send_rescuer_digital_twin_to_scene_logging_blocked(t, e);
	}

	// ho trovato tutto! l'emergenza non ha più missing rescuers
	for (int i = 0; i < e->type->rescuers_req_number; i++) {
		e->rescuers_missing[i]->required_count = 0;
	}
	
	unlock_system();
	return found;
}

void send_rescuer_digital_twin_to_scene_logging_blocked(rescuer_digital_twin_t *t, emergency_t *e){
	switch (t->status) {
		case IDLE: 				LOG_RESCUER_SENT_NAME(t, e->x, e->y, e->type->emergency_desc, "inizia il viaggio base -> emergenza"); break;
		case RETURNING_TO_BASE: LOG_RESCUER_SENT_NAME(t, e->x, e->y, e->type->emergency_desc, "andava alla base ma ora da emergenza"); break;
		case ON_SCENE: 
			if(t->emergency->id == e->id) return; // siamo già su quella scena!
			LOG_RESCUER_SENT_NAME(t, e->x, e->y, e->type->emergency_desc, "parte da una scena per andare da un'altra");
			break;
		case EN_ROUTE_TO_SCENE:
			if(t->emergency->id == e->id) return; // stiamo già andando su quella scena!
			LOG_RESCUER_SENT_NAME(t, e->x, e->y, e->type->emergency_desc, "andava in una scena ma cambia e va da un'altra");
			break;
		default: LOG_RESCUER_SENT_NAME(t, e->x, e->y, e->type->emergency_desc, "parte verso la scena dell'emergenza"); break;
	}
	t->status = EN_ROUTE_TO_SCENE; // cambio lo stato del twin
	t->emergency = e;
	rescuer_request_t *request = get_rescuer_request_by_name(t->rescuer->rescuer_type_name, e->type->rescuers);
	t->time_to_manage = request->time_to_manage;
	t->time_left_before_leaving = request->time_to_manage;
	change_rescuer_digital_twin_destination_blocked(t, e->x, e->y);
}
