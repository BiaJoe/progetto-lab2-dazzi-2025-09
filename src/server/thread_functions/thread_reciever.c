#include "server.h"

// ----------- funzioni per il thread reciever -----------

// thread function che riceve le emergenze
// le scarta se sbagliate
// le inserisce nella queue
// sarà eseguita nel main thread del server
int thread_reciever(void *arg){
	server_context_t *ctx = arg;
	log_event(NON_APPLICABLE_LOG_ID, MESSAGE_QUEUE_SERVER, "inizio della ricezione delle emergenze!");
	char buffer[MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH];

	while (1) {
		check_error_mq_receive(mq_receive(ctx->mq, buffer, sizeof(buffer), NULL));
		if (strcmp(buffer, STOP_MESSAGE_FROM_CLIENT) == 0) {		// se è il messaggio di stop devo uscire!
			ctx -> server_must_stop = true;
			return 0;
		}

		ctx->requests->count++;
		emergency_request_t *r = parse_emergency_request(buffer, ctx->emergencies->types, ctx->enviroment->height, ctx->enviroment->width);
		if(!r) {
			log_event(ctx->requests->count, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, "emergenza %s rifiutata perchè non conforme alla sintassi", buffer);
			continue;
		}

		ctx->requests->valid_count++;
		log_event(ctx->requests->valid_count, EMERGENCY_REQUEST_RECEIVED, "emergenza %s (%d, %d) %ld ricevuta e messa in attesa di essere processata!", r->emergency_name, r->x, r->y, r->timestamp);

		// emergency_t *emergency = mallocate_emergency(ctx->emergencies->count++, ctx->emergencies->types, r);
		free(r);

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
	e->priority = type->priority; 					// prendo la priorità dall'emergency_type, potrei doverla modificare in futuri
	e->type = type;
	e->status = status;
	e->x = r->x;
	e->y = r->y;
	e->time = r->timestamp;
	e->rescuer_count = rescuer_count;
	e->rescuer_twins = rescuer_twins;
	e->time_since_started_waiting = INVALID_TIME;
	e->time_since_it_was_assigned = INVALID_TIME;
	e->time_spent_existing = 0;

	return e;
}

void free_emergency(emergency_t* e){
	if(!e) return;
	free(e->rescuer_twins);			// libero il puntatore ai gemelli digitali dei rescuers (non i gemelli stessi ovviamente)
	free(e);
}

