#include "emergency_requests_reciever.h"

// ----------- funzioni per il thread reciever -----------

// thread function che riceve le emergenze
// le scarta se sbagliate
// le inserisce nella queue
// sarà eseguita nel main thread del server
int emergency_requests_reciever(server_context_t *ctx){
	log_event(NON_APPLICABLE_LOG_ID, MESSAGE_QUEUE_SERVER, "inizio della ricezione delle emergenze!");
	char buffer[MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH];

	while (1) {
		check_error_mq_receive(mq_receive(ctx->mq, buffer, sizeof(buffer), NULL));
		if (strcmp(buffer, STOP_MESSAGE_FROM_CLIENT) == 0) {		// se è il messaggio di stop devo uscire!
			ctx -> server_must_stop = true;
			return 0;
		}

		ctx->emergency_requests_count++;
		char name[EMERGENCY_NAME_LENGTH]; int x, y; time_t time;
		if(!parse_emergency_request(buffer, name, &x, &y, &time) || emergency_request_values_are_illegal(ctx, name, x, y, time)){ 
			log_event(ctx->emergency_requests_count, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, "emergenza %s (%d, %d) %ld rifiutata perchè conteneva valori illegali", name, x, y, time);
			continue;
		}
		
		ctx->valid_emergency_request_count++;
		emergency_queue_t *q = ctx->waiting_queue;
		emergency_t *e = mallocate_emergency(ctx, name, x, y, time);
		emergency_node_t *n = mallocate_emergency_node(e); 	
		n->emergency->time_since_started_waiting = 0; // il timer dell'emergenza inizia a scorrere
		
		lock_queue(q);
		enqueue_emergency_node(q, n);
		unlock_queue(q);										

		log_event(ctx->valid_emergency_request_count, EMERGENCY_REQUEST_RECEIVED, "emergenza %s (%d, %d) %ld ricevuta e messa in attesa di essere processata!", name, x, y, time);
	}
	return 0;
}

int parse_emergency_request(char *message, char* name, int *x, int *y, time_t *timestamp){
	if(sscanf(message, EMERGENCY_REQUEST_SYNTAX, name, x, y, timestamp) != 4)
		return 0;
	return 1;
}

int emergency_request_values_are_illegal(server_context_t *ctx, char* name, int x, int y, time_t timestamp){
	if(strlen(name) <= 0) return YES;
	int h = ctx->height;
	int w = ctx->width;
	if(!get_emergency_type_by_name(name, ctx->emergency_types)) return YES;
	if(ABS(x) < MIN_X_COORDINATE_ABSOLUTE_VALUE || ABS(x) > ABS(w)) return YES;
	if(ABS(y) < MIN_Y_COORDINATE_ABSOLUTE_VALUE || ABS(y) > ABS(h)) return YES;
	if(timestamp == INVALID_TIME) return YES;
	return NO;
}