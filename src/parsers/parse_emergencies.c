#include "parsers.h"

static int rescuer_requests_total_count = 0;

void parse_emergencies(server_context_t *ctx){
	parsing_state_t *ps = mallocate_parsing_state(EMERGENCY_TYPES_CONF);
	emergency_type_t **emergency_types = callocate_emergency_types();	
	rescuer_type_t **rescuer_types = ctx->rescuer_types;
	emergency_type_fields_t fields = {0};																																																				

	while (go_to_next_line(ps)) {
		log_and_fail_if_file_line_cant_be_processed(ps, MAX_EMERGENCY_CONF_LINES, MAX_EMERGENCY_TYPES_COUNT, MAX_EMERGENCY_CONF_LINE_LENGTH);
		if (check_and_log_if_line_is_empty(ps)) 
			continue;
		if (!check_and_extract_emergency_type_fields_from_line(ps, emergency_types, rescuer_types, &fields))
			continue;
		emergency_types[ps->parsed_so_far++] = mallocate_and_populate_emergency_type(&fields); // metto l'emergenza nell'array di emergenze
		log_event(ps->parsed_so_far, EMERGENCY_PARSED, "⚠️ Emergenza %s di priorità %hd con %d tipi di rescuer registrata tra i tipi di emergenze", fields.emergency_desc,  fields.priority,  fields.rescuers_req_number);
	}
	ctx -> emergency_types_count = ps->parsed_so_far; // restituisco il numero di emergenze lette
	ctx -> emergency_types = emergency_types;	
	free_parsing_state(ps);
}

int check_and_extract_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_t **emergency_types, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields){
	check_and_extract_simple_emergency_type_fields_from_line(ps, fields);
	if(check_and_log_if_emergency_type_already_parsed(ps, emergency_types, fields))
		return NO;
	check_and_extract_rescuer_requests_from_string(ps, rescuer_types, fields);
	return YES;
}

int check_and_log_if_emergency_type_already_parsed(parsing_state_t *ps, emergency_type_t**types, emergency_type_fields_t* fields){
	if(get_emergency_type_by_name(fields->emergency_desc, types)){ 																																					// Controllo che non ci siano duplicati, se ci sono ignoro la riga
		log_event(ps->line_number, DUPLICATE_EMERGENCY_TYPE_IGNORED, "L'emergenza %s a linea %d del file %s è gìa stata aggiunta, ignorata", fields->emergency_desc, ps->line_number, ps->filename);
		return YES;
	}
	return NO;
}

void check_and_extract_simple_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_fields_t *fields){
	if(
		sscanf(ps->line, EMERGENCY_TYPE_SYNTAX, fields->emergency_desc, &(fields->priority), fields->rescuer_requests_string) !=3 ||
		emergency_values_are_illegal(fields)
	)	{
		log_fatal_error(LINE_FILE_ERROR_STRING "%s", ps->line_number, ps->filename, ps->line);
	}
}

void check_and_extract_rescuer_request_fields_from_token(parsing_state_t *ps, int requests_parsed_so_far, char*token, char *name, int *count, int *time){
	if(requests_parsed_so_far > MAX_RESCUER_REQ_NUMBER_PER_EMERGENCY)		
		log_fatal_error(LINE_FILE_ERROR_STRING "Numero massimo di rescuer richiesti per una sola emergenza superato", ps->line_number, ps->filename);	
	if(sscanf(token, RESCUER_REQUEST_SYNTAX, name, count, time) != 3) 	
		log_fatal_error(LINE_FILE_ERROR_STRING "la richiesta del rescuer numero %d è errata", ps->line_number, ps->filename, requests_parsed_so_far);	
	if(rescuer_request_values_are_illegal(name, *count, *time))					
		log_fatal_error(LINE_FILE_ERROR_STRING "la richiesta del rescuer numero %d contiene valori illegali", ps->line_number, ps->filename, requests_parsed_so_far);	
}

int check_and_log_if_rescuer_request_already_parsed(parsing_state_t *ps, rescuer_request_t **requests, char* name){
	if(get_rescuer_request_by_name(name, requests)){ 																																				
		log_event(ps->line_number, DUPLICATE_RESCUER_REQUEST_IGNORED, "Linea %d file %s: il rescuer %s è gìa stato richesto dall' emergenza, ignorato", ps->line_number, ps->filename, name);
		return YES;
	}
	return NO;	
}

void check_if_rescuer_requested_is_available(parsing_state_t *ps, rescuer_type_t **types, char *name, int count){
	rescuer_type_t *type = get_rescuer_type_by_name(name, types);
	if(!type) log_fatal_error(LINE_FILE_ERROR_STRING "Richiesto rescuer inesistente %s", ps->line_number, ps->filename, name);	
	if(count > type->amount) log_fatal_error(LINE_FILE_ERROR_STRING "Numero di rescuer richiesti superiore a quelli disponibili (%d > %d)", ps->line_number, ps->filename, count, type->amount);	
}

void check_and_extract_rescuer_requests_from_string(parsing_state_t *ps, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields){
	rescuer_request_t **requests = callocate_rescuer_requests();
	char name[MAX_RESCUER_NAME_LENGTH];			// buffer per il nome del rescuer
	int required_count, time_to_manage; 		// variabili temporanee per i campi del rescuer
	int requests_parsed_so_far = 0;					// aumenta ad ogni rescuer registrato e rappresenta anche l'indice dove mettere la richiesta nell'array
	for(char *token = strtok(fields->rescuer_requests_string, RESCUER_REQUESTS_SEPARATOR); token != NULL; token = strtok(NULL, RESCUER_REQUESTS_SEPARATOR)){
		check_and_extract_rescuer_request_fields_from_token(ps, requests_parsed_so_far, token, name, &required_count, &time_to_manage);
		if(check_and_log_if_rescuer_request_already_parsed(ps, requests, name))
			continue;
		check_if_rescuer_requested_is_available(ps, rescuer_types, name, required_count);
		requests[requests_parsed_so_far] = mallocate_and_populate_rescuer_request(required_count, time_to_manage, get_rescuer_type_by_name(name, rescuer_types));	
		log_event(rescuer_requests_total_count, RESCUER_REQUEST_ADDED, "Richiesta di %d unità di %s registrate a linea %d del file %s", required_count, name, ps->line_number, ps->filename);
		requests_parsed_so_far++;														
		rescuer_requests_total_count++;
	}
	fields->rescuers_req_number = requests_parsed_so_far;
	fields->rescuers = requests;
}

int rescuer_request_values_are_illegal(char *name, int required_count, int time_to_manage){
	return (
		strlen(name) <= 0 || 
		required_count < MIN_RESCUER_REQUIRED_COUNT || 
		required_count > MAX_RESCUER_REQUIRED_COUNT || 
		time_to_manage < MIN_TIME_TO_MANAGE || 
		time_to_manage > MAX_TIME_TO_MANAGE
	);
}

int emergency_values_are_illegal(emergency_type_fields_t *fields){
	return (
		strlen(fields->emergency_desc) <= 0 || 
		fields->priority < MIN_EMERGENCY_PRIORITY || 
		fields->priority > MAX_EMERGENCY_PRIORITY
	);
}

// memory management

emergency_type_t ** callocate_emergency_types(){
	emergency_type_t **emergency_types = (emergency_type_t **)calloc(MAX_EMERGENCY_TYPES_COUNT + 1, sizeof(emergency_type_t *));
	check_error_memory_allocation(emergency_types);
	return emergency_types;
}

rescuer_request_t **callocate_rescuer_requests(){
	rescuer_request_t **rescuer_requests = (rescuer_request_t **)calloc(MAX_RESCUER_REQ_NUMBER_PER_EMERGENCY, sizeof(rescuer_request_t*));
	check_error_memory_allocation(rescuer_requests);
	return rescuer_requests;
}

emergency_type_t *mallocate_emergency_type(){
	emergency_type_t *e = (emergency_type_t *)malloc(sizeof(emergency_type_t));	
	check_error_memory_allocation(e);
	e->emergency_desc = (char *)calloc((EMERGENCY_NAME_LENGTH + 1), sizeof(char));		
	check_error_memory_allocation(e->emergency_desc);
	e->priority = 0;
	e->rescuers_req_number = 0;
	e->rescuers = NULL;
	return e;
}

emergency_type_t *mallocate_and_populate_emergency_type(emergency_type_fields_t *fields){
	emergency_type_t *e = (emergency_type_t *)malloc(sizeof(emergency_type_t));			// allco l'emergency_type_t
	check_error_memory_allocation(e);
	e->emergency_desc = (char *)malloc((strlen(fields->emergency_desc) + 1) * sizeof(char));		
	check_error_memory_allocation(e->emergency_desc);							// alloco il nome dell'emergency_type_t e lo copio
	strcpy(e->emergency_desc, fields->emergency_desc);
	e->priority = fields->priority;																				// popolo il resto dei campi
	e->rescuers_req_number = fields->rescuers_req_number;
	e->rescuers = fields->rescuers;																				// assegno i rescuer richiesti
	return e;
}

rescuer_request_t *mallocate_and_populate_rescuer_request(int required_count, int time_to_manage, rescuer_type_t *type){
	rescuer_request_t *r = (rescuer_request_t *)malloc(sizeof(rescuer_request_t));
	check_error_memory_allocation(r);
	r->type = type;
	r->required_count = required_count;
	r->time_to_manage = time_to_manage;
	return r;
}



