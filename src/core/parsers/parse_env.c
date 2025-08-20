#include "parsers.h"

environment_t *parse_env(){
	parsing_state_t *ps = mallocate_parsing_state(ENV_CONF);
	environment_t* env = mallocate_enviroment();
	int number_of_lines_to_parse = ENV_CONF_VALID_LINES_COUNT;

    check_and_extract_env_line_field(ps, env, ENV_CONF_QUEUE_SYNTAX);
    check_and_extract_env_line_field(ps, env, ENV_CONF_HEIGHT_SYNTAX);
    check_and_extract_env_line_field(ps, env, ENV_CONF_WIDTH_SYNTAX);

    if (environment_values_are_illegal(env))
		log_fatal_error("Valori illegali nel file di configurazione: %s", ps->filename);

	if(ps->parsed_so_far != number_of_lines_to_parse)
		log_fatal_error("il file %s non è scritto correttamente, dovrebbe avere %d righe, ma ne ha %d", ps->filename, number_of_lines_to_parse, ps->parsed_so_far);
	
	free_parsing_state(ps);
    return env;
}

void check_and_extract_env_line_field(parsing_state_t *ps, environment_t *e, const char *syntax){
    skip_and_log_empty_or_illegal_lines(ps, MAX_ENV_CONF_LINES, ENV_CONF_VALID_LINES_COUNT, MAX_ENV_CONF_LINE_LENGTH);
    if (sscanf(ps->line, syntax, e->queue_name) != 1)
        log_fatal_error(LINE_FILE_ERROR_STRING "linea contenente errori sintattici %s", ps->line_number, ps->filename, ps->line);
}

bool environment_values_are_illegal(environment_t *e){
	return (
		ABS(e->height) < MIN_Y_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->height) > MAX_Y_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->width) < MIN_X_COORDINATE_ABSOLUTE_VALUE || 
		ABS(e->width) > MAX_X_COORDINATE_ABSOLUTE_VALUE
	) ? true : false;
}





