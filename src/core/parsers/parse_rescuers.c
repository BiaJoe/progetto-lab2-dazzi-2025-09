#include "parsers.h"

static int rescuer_digital_twins_total_count = 0;								// conto dei gemelli digitali, necessario per il logging

rescuers_t *parse_rescuers(char *filename, int x, int y){
	parsing_state_t *ps = mallocate_parsing_state(filename);
	rescuer_type_t **rescuer_types = callocate_rescuer_types();		
	rescuer_type_fields_t fields = {0};		

	rescuers_t *rescuers = (rescuers_t *)calloc(1, sizeof(rescuers_t));
	check_error_memory_allocation(rescuers);

	fields.max_x_coordinate = x;
	fields.max_y_coordinate = y;	
	
	// Leggo ogni riga del file e processo le informazioni contenute
	while (go_to_next_line(ps)) {	

		if(!check_if_line_can_be_processed_logging(ps, MAX_RESCUER_CONF_LINES, MAX_RESCUER_TYPES_COUNT, MAX_RESCUER_CONF_LINE_LENGTH)){
			if (ps->should_continue_parsing) continue;		
			free_parsing_state(ps);	
			return NULL;
		}
		
		if(!check_and_extract_rescuer_type_fields_from_line(ps, rescuer_types, &fields)){
			if (ps->should_continue_parsing) continue;
			free_parsing_state(ps);	
			return NULL;
		}

		rescuer_types[ps->parsed_so_far++] = mallocate_and_populate_rescuer_type(&fields); 							// i campi sono validi e il nome non è presente, alloco il rescuer 
		log_event(ps->line_number, RESCUER_TYPE_PARSED, "🚨  Rescuer (%d, %d) %s e %d gemelli digitali aggiunto!", fields.x, fields.y, fields.name, fields.amount);
	}
	rescuers -> count = ps->parsed_so_far;
	rescuers -> types = rescuer_types;
	free_parsing_state(ps);
	return rescuers;
}

bool check_and_extract_rescuer_type_fields_from_line(parsing_state_t *ps, rescuer_type_t **rescuer_types, rescuer_type_fields_t *fields){
	if (sscanf(ps->line, RESCUERS_SYNTAX, fields->name, &(fields->amount), &(fields->speed), &(fields->x), &(fields->y)) != 5){																													
		log_parsing_error(LINE_FILE_ERROR_STRING "sintassi. %s", ps->line_number, ps->filename, ps->line);
		ps->should_continue_parsing = false;
		return false;
	}
	if (rescuer_type_values_are_illegal(fields)){
		log_parsing_error(LINE_FILE_ERROR_STRING "valori illegali. %s", ps->line_number, ps->filename, ps->line);
		ps->should_continue_parsing = false;
		return false;
	}
	if(get_rescuer_type_by_name(fields->name, rescuer_types)){ 																																					// Controllo che non ci siano duplicati, se ci sono ignoro la riga
		log_event(ps->line_number, DUPLICATE_RESCUER_TYPE_IGNORED, "Linea %d file %s: il rescuer %s è gìa stato aggiunto, ignorato", ps->line_number, ps->filename, fields->name);
		return false;
	}
	return true;
}

bool check_and_log_if_rescuer_type_already_parsed(parsing_state_t *ps, rescuer_type_t **types, char* name){
	if(get_rescuer_type_by_name(name, types)){ 																																					// Controllo che non ci siano duplicati, se ci sono ignoro la riga
		log_event(ps->line_number, DUPLICATE_RESCUER_TYPE_IGNORED, "Linea %d file %s: il rescuer %s è gìa stato aggiunto, ignorato", ps->line_number, ps->filename, name);
		return true;
	}
	return false;
}

bool rescuer_type_values_are_illegal(rescuer_type_fields_t *fields){
	return (
		strlen(fields->name) <= 0 || 
		fields->amount < MIN_RESCUER_AMOUNT || 
		fields->amount > MAX_RESCUER_AMOUNT || 
		fields->speed < MIN_RESCUER_SPEED || 
		fields->speed > MAX_RESCUER_SPEED || 
		fields->x < MIN_X_COORDINATE_ABSOLUTE_VALUE || 
		fields->x > fields->max_x_coordinate || 
		fields->y < MIN_Y_COORDINATE_ABSOLUTE_VALUE || 
		fields->y > fields->max_y_coordinate
	) ? true : false;
}

rescuer_type_t ** callocate_rescuer_types(){
	rescuer_type_t **rescuer_types = (rescuer_type_t **)calloc((MAX_RESCUER_CONF_LINES + 1),  sizeof(rescuer_type_t*));
	check_error_memory_allocation(rescuer_types);
	return rescuer_types;
}

rescuer_type_t *mallocate_and_populate_rescuer_type(rescuer_type_fields_t *fields){
	rescuer_type_t *r = (rescuer_type_t *)malloc(sizeof(rescuer_type_t));
	check_error_memory_allocation(r);
	r->rescuer_type_name = (char *)malloc((strlen(fields->name) + 1) * sizeof(char));	// alloco il nome del rescuer_type_t e lo copio
	check_error_memory_allocation(r->rescuer_type_name);
	strcpy(r->rescuer_type_name, fields->name);																				// popolo i campi del rescuer type
	trim_right(r->rescuer_type_name);
	r->amount = fields->amount;									
	r->speed = fields->speed;										
	r->x = fields->x;											
	r->y = fields->y;
	r->twins = callocate_and_populate_rescuer_digital_twins(r);
	return r;
}

rescuer_digital_twin_t **callocate_and_populate_rescuer_digital_twins(rescuer_type_t* r){
	rescuer_digital_twin_t **twins = (rescuer_digital_twin_t **)calloc((size_t)r->amount + 1, sizeof(rescuer_digital_twin_t*));
	check_error_memory_allocation(twins);
	for(int i = 0; i < r->amount; i++){
		twins[i] = mallocate_rescuer_digital_twin(r);
		log_event(twins[i]->id, RESCUER_DIGITAL_TWIN_ADDED, "⛑️  Gemello [%d, %d] (ID:%d) %s creato ", twins[i]->x, twins[i]->y, twins[i]->id,  r->rescuer_type_name);
	}
	return twins;
}

rescuer_digital_twin_t *mallocate_rescuer_digital_twin(rescuer_type_t* r){
	rescuer_digital_twin_t *t = (rescuer_digital_twin_t *)malloc(sizeof(rescuer_digital_twin_t));
	check_error_memory_allocation(t);
	t->id 				= rescuer_digital_twins_total_count++;		// in questo modo ogni gemello è unico
	t->x 				= r->x;
	t->y 				= r->y;
	t->rescuer 			= r;
	t->status 			= IDLE;
	t->trajectory		= mallocate_bresenham_trajectory(); 		// non inizializzato perchè nessun valore è sensato prima di aver dato una destinazione reale
	t->emergency 		= NULL; 																// il gemello viene messo nella base all'inizio, quindi in effetti è arrivato
	t->time_to_manage	= INVALID_TIME;
	t->time_left_before_leaving = INVALID_TIME;
	t->is_a_candidate  = false;
	return t;
}

