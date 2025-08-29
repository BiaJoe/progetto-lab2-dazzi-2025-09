#include "parsers.h"

environment_t *parse_env(char *filename){
	parsing_state_t *ps = mallocate_parsing_state(filename);
	environment_t* env = mallocate_enviroment();
	int number_of_lines_to_parse = ENV_CONF_VALID_LINES_COUNT;

    check_and_extract_env_line_field(ps, env->queue_name, ENV_CONF_QUEUE_SYNTAX);
    check_and_extract_env_line_field(ps, &env->height, ENV_CONF_HEIGHT_SYNTAX);
    check_and_extract_env_line_field(ps, &env->width, ENV_CONF_WIDTH_SYNTAX);

	cleanup_queue_name(env->queue_name);

    if (environment_values_are_illegal(env)){	
		log_parsing_error("Valori illegali nel file di configurazione: %s", ps->filename);
		return NULL;
	}

	if(ps->parsed_so_far != number_of_lines_to_parse){
		log_parsing_error("il file %s non è scritto correttamente, dovrebbe avere %d righe, ma ne ha %d", ps->filename, number_of_lines_to_parse, ps->parsed_so_far);
		return NULL;
	}	
	
	free_parsing_state(ps);
    return env;
}

void check_and_extract_env_line_field(parsing_state_t *ps, void *variable_ptr, const char *syntax){
	skip_and_log_empty_or_illegal_lines(ps, MAX_ENV_CONF_LINES, ENV_CONF_VALID_LINES_COUNT, MAX_ENV_CONF_LINE_LENGTH);
	if (sscanf(ps->line, syntax, variable_ptr) != 1){
        log_parsing_error(LINE_FILE_ERROR_STRING "linea contenente errori sintattici %s", ps->line_number, ps->filename, ps->line);
	}
	ps->parsed_so_far++;
}

bool environment_values_are_illegal(environment_t *e){
	return (
		strlen(e->queue_name) < 2 ||
		strlen(e->queue_name) > MAX_EMERGENCY_QUEUE_NAME_LENGTH ||
		ABS(e->height) < MIN_Y_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->height) > MAX_Y_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->width) < MIN_X_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->width) > MAX_X_COORDINATE_ABSOLUTE_VALUE
	) ? true : false;
}

void cleanup_queue_name(char* qn){
	bool slashes = qn[0] == '/' ? true : false;
	int len = (int)strlen(qn);
	for (int i = 1; i < len; i++){
		if (qn[i] == '/') qn[i] = '-';
	}
	if (slashes) return; // se c'era già lo slash all'inizio siamo a posto
	// altrimenti dobbiamo mettercelo in place
	
	for (int i = len-1; i > 0; i--) {
		qn[i] = qn[i-1];
	}

	qn[0] = '/';
	qn[len] = '\0';
}





