#include "parsers.h"

void parse_env(server_context_t *ctx){
	parsing_state_t *ps = mallocate_parsing_state(ENV_CONF);
	environment_fields_t e = {0};
	int number_of_lines_to_parse = ENV_CONF_VALID_LINES_COUNT;

	while(go_to_next_line(ps)){
		log_and_fail_if_file_line_cant_be_processed(ps, MAX_ENV_CONF_LINES, ENV_CONF_VALID_LINES_COUNT, MAX_ENV_CONF_LINE_LENGTH);
		if(check_and_log_if_line_is_empty(ps)) continue;
		check_and_extract_env_line_field(ps, &e);
	}
	
	if(ps->parsed_so_far != number_of_lines_to_parse)
		log_fatal_error("il file %s non è scritto correttamente, dovrebbe avere %d righe, ma ne ha %d", ps->filename, number_of_lines_to_parse, ps->parsed_so_far);
	
	ctx->height = e.height;
	ctx->width  = e.width;
	free_parsing_state(ps);
}

void check_and_extract_env_line_field(parsing_state_t *ps, environment_fields_t *e){
	int must_be_one = 0;
	switch (ps->parsed_so_far++) {
		case ENV_CONF_QUEUE_TURN:  must_be_one = sscanf(ps->line, ENV_CONF_QUEUE_SYNTAX, e->queue_name); 	break;
		case ENV_CONF_HEIGHT_TURN: must_be_one = sscanf(ps->line, ENV_CONF_HEIGHT_SYNTAX, &e->height); 		break;
		case ENV_CONF_WIDTH_TURN:  must_be_one = sscanf(ps->line, ENV_CONF_WIDTH_SYNTAX, &e->width); 			break;		
		default: log_fatal_error(LINE_FILE_ERROR_STRING "linea non riconosciuta %s", ps->line_number, ps->filename, ps->line);
	}
	if (must_be_one != 1)
		log_fatal_error(LINE_FILE_ERROR_STRING "linea contenente errori sintattici %s", ps->line_number, ps->filename, ps->line);
	if (environment_values_are_illegal(e))
		log_fatal_error(LINE_FILE_ERROR_STRING "linea contenente valori errati %s", ps->line_number, ps->filename, ps->line);
}

int environment_values_are_illegal(environment_fields_t *e){
	return (
		strcmp(e->queue_name, EMERGENCY_QUEUE_NAME) != 0 ||
		ABS(e->height) < MIN_Y_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->height) > MAX_Y_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->width) < MIN_X_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->width) > MAX_X_COORDINATE_ABSOLUTE_VALUE
	);
}