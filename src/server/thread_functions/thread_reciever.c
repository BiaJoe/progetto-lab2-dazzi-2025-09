#include "server.h"

// ----------- funzioni per il thread reciever -----------

// thread function che riceve le emergenze
// le scarta se sbagliate
// le inserisce nella queue
// sarà eseguita nel main thread del server
int thread_reciever(void *arg){
	log_event(NON_APPLICABLE_LOG_ID, MESSAGE_QUEUE_SERVER, "inizio della ricezione delle emergenze!");
	char buffer[MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH];

	while (!atomic_load(&ctx->server_must_stop)) {
		ssize_t nread = mq_receive(ctx->mq, buffer, sizeof(buffer), NULL);
    	check_error_mq_receive(nread);

    	if (nread < 0) { 
			log_event(NON_APPLICABLE_LOG_ID, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, "Thread reciever non ha letto niente!");
		}

		if ((size_t)nread >= sizeof(buffer)) {
			buffer[sizeof(buffer) - 1] = '\0';
			log_event(NON_APPLICABLE_LOG_ID, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, "messaggio MQ troncato: \"%s\"", buffer);
			continue;
		}	

		buffer[nread] = '\0'; 

		if (strcmp(buffer, MQ_STOP_MESSAGE) == 0) {		// se è il messaggio di stop devo uscire!
			atomic_store(&ctx->server_must_stop, true);
			return 0;
		}

		ctx->requests->count++;
		emergency_request_t *r = parse_emergency_request(buffer, ctx->emergencies->types, ctx->enviroment->height, ctx->enviroment->width);
		if(!r) {
			log_event(ctx->requests->count, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, "emergenza \"%s\" rifiutata perchè non conforme alla sintassi", buffer);
			continue;
		}

		ctx->requests->valid_count++;
		emergency_t *e = mallocate_emergency(ctx->emergencies->count++, ctx->emergencies->types, r);
		pq_push(ctx->emergency_queue, e, priority_to_level(e->priority));
		log_event(ctx->requests->valid_count, EMERGENCY_REQUEST_RECEIVED, "emergenza %s (%d, %d) P:%d R:%d ricevuta!", e->type->emergency_desc, e->x, e->y, (int)e->priority, e->rescuer_count);
		free(r);
		r = NULL;
	}
	return 0;
}


emergency_t *mallocate_emergency(int id, emergency_type_t **types, emergency_request_t *r){
	emergency_t *e = (emergency_t *)malloc(sizeof(emergency_t));
	check_error_memory_allocation(e);
	emergency_type_t *type = get_emergency_type_by_name(r->emergency_name, types);
	emergency_status_t status = WAITING;
	int rescuer_count = 0;
	for(int i = 0; i < type->rescuers_req_number; i++)		// calcola il numero totale di rescuer che servono all'emergenza
		rescuer_count += type->rescuers[i]->required_count;
	rescuer_digital_twin_t **rescuer_twins = (rescuer_digital_twin_t **)calloc((size_t)rescuer_count + 1, sizeof(rescuer_digital_twin_t *));
	check_error_memory_allocation(rescuer_twins);			// alloca il numero necessario di rescuers (per ora tutti a NULL)

	e->id = id;
	e->priority = type->priority; 							// prendo la priorità dall'emergency_type, potrei doverla modificare in futuri
	e->type = type;
	e->status = status;
	e->x = r->x;
	e->y = r->y;
	e->time = r->timestamp;
	e->rescuer_count = rescuer_count;
	e->rescuer_twins = rescuer_twins;
	
	atomic_store(&e->has_been_paused, false);
	atomic_store(&e->rescuers_not_done_yet, e->rescuer_count);
	atomic_store(&e->rescuers_not_arrived_yet, e->rescuer_count);
	atomic_store(&e->tick_time, get_current_tick_count(ctx));
	atomic_store(&e->timeout_timer, priority_to_timeout_timer(e->priority));
	atomic_store(&e->promotion_timer, priority_to_promotion_timer(e->priority));

	check_error_mtx_init(mtx_init(&e->mutex, mtx_plain));
	check_error_cnd_init(cnd_init(&e->cond));
	return e;
}

void free_emergency(emergency_t* e){
	if(!e) return;
	free(e->rescuer_twins);			// libero il puntatore ai gemelli digitali dei rescuers (non i gemelli stessi ovviamente)
	free(e);
}

