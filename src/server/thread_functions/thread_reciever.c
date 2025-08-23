#include "server.h"

// ----------- funzioni per il thread reciever -----------

// thread function che riceve le emergenze
// le scarta se sbagliate
// le inserisce nella queue
// sarà eseguita nel main thread del server
int thread_reciever(server_context_t *ctx){
	log_event(NON_APPLICABLE_LOG_ID, MESSAGE_QUEUE_SERVER, "inizio della ricezione delle emergenze!");
	char buffer[MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH];

	while (1) {
		check_error_mq_receive(mq_receive(ctx->mq, buffer, sizeof(buffer), NULL));
		if (strcmp(buffer, STOP_MESSAGE_FROM_CLIENT) == 0) {		// se è il messaggio di stop devo uscire!
			ctx -> server_must_stop = true;
			return 0;
		}

		ctx->requests->count++;
		char name[MAX_EMERGENCY_NAME_LENGTH]; int x, y; time_t time;
		if(!parse_emergency_request(buffer, name, &x, &y, &time) || emergency_request_values_are_illegal(ctx, name, x, y, time)){ 
			log_event(ctx->requests->count, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, "emergenza %s (%d, %d) %ld rifiutata perchè conteneva valori illegali", name, x, y, time);
			continue;
		}
		
		ctx->requests->valid_count++;
		
		log_event(ctx->requests->valid_count, EMERGENCY_REQUEST_RECEIVED, "emergenza %s (%d, %d) %ld ricevuta e messa in attesa di essere processata!", name, x, y, time);
	}
	return 0;
}

int parse_emergency_request(char *message, char* name, int *x, int *y, time_t *timestamp){
	if(sscanf(message, EMERGENCY_REQUEST_SYNTAX, name, x, y, timestamp) != 4)
		return 0;
	return 1;
}

bool emergency_request_values_are_illegal(server_context_t *ctx, char* name, int x, int y, time_t timestamp){
	if(strlen(name) <= 0) return true;
	int h = ctx->enviroment->height;
	int w = ctx->enviroment->width;
	if(!get_emergency_type_by_name(name, ctx->emergencies->types)) return true;
	if(ABS(x) < MIN_X_COORDINATE_ABSOLUTE_VALUE || ABS(x) > ABS(w)) return true;
	if(ABS(y) < MIN_Y_COORDINATE_ABSOLUTE_VALUE || ABS(y) > ABS(h)) return true;
	if(timestamp == INVALID_TIME) return true;
	return false;
}


emergency_t *mallocate_emergency(server_context_t *ctx, char* name, int x, int y, time_t timestamp){
	emergency_t *e = (emergency_t *)malloc(sizeof(emergency_t));
	check_error_memory_allocation(e);
	emergency_type_t *type = get_emergency_type_by_name(name, ctx->emergencies->types);
	emergency_status_t status = WAITING;
	int rescuer_count = 0;
	for(int i = 0; i < type->rescuers_req_number; i++)		// calcola il numero totale di rescuer che servono all'emergenza
		rescuer_count += type->rescuers[i]->required_count;
	rescuer_digital_twin_t **rescuer_twins = (rescuer_digital_twin_t **)calloc(rescuer_count + 1, sizeof(rescuer_digital_twin_t *));
	check_error_memory_allocation(rescuer_twins);			// alloca il numero necessario di rescuers (per ora tutti a NULL)

	e->id = ctx->requests->valid_count;
	e->priority = (int) type->priority; 					// prendo la priorità dall'emergency_type, potrei doverla modificare in futuri
	e->type = type;
	e->status = status;
	e->x = x;
	e->y = y;
	e->time = timestamp;
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

