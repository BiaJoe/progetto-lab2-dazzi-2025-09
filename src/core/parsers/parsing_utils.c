#include "parsers.h"

parsing_state_t *mallocate_parsing_state(char *filename){
	parsing_state_t *ps = (parsing_state_t *)malloc(sizeof(parsing_state_t));
	check_error_memory_allocation(ps);
	ps->fp = fopen(filename, "r");					
	check_opened_file_error_log(ps->fp);	
	ps->filename = strdup(filename); // alloco e copio il filename
  check_error_memory_allocation(ps->filename);
	ps->line = NULL; 
	ps->len = 0;
	ps->line_number = 0;
	ps->parsed_so_far = 0;
	return ps;
}

void free_parsing_state(parsing_state_t *ps){
	if (ps->fp) 	  fclose(ps->fp);
	if (ps->filename) free(ps->filename);
	if (ps->line) 	  free(ps->line);
	free(ps);
}

environment_t *mallocate_enviroment(){
	environment_t *env = (environment_t *)malloc(sizeof(environment_t));
	check_error_memory_allocation(env);
	env->queue_name[0] = '\0'; 
	env->height = 0;
	env->width = 0;
	return env;	
}

bool go_to_next_line(parsing_state_t *ps){
	if (getline(&(ps->line), &(ps->len), ps->fp) == -1) 
		return false;
	ps->line_number++;
	return true;
}

bool check_and_log_if_line_is_empty(parsing_state_t *ps){
	if (is_line_empty(ps->line)){
		log_event(AUTOMATIC_LOG_ID, EMPTY_CONF_LINE_IGNORED, "linea %d in %s vuota, ignorata", ps->line_number, ps->filename);
		return true;
	}
	return false;
}

void log_and_fail_if_file_line_cant_be_processed(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length){
	if(ps->line_number > max_lines)				 log_fatal_error("Numero massimo di linee superato in %s", ps->filename);
	if(ps->parsed_so_far > max_parsable_lines)	 log_fatal_error("Numero massimo di linee elaborate superato in %s", ps->filename);
	if((int) strlen(ps->line) > max_line_length) log_fatal_error(LINE_FILE_ERROR_STRING "Linea troppo lunga", ps->line_number, ps->filename);
}

void skip_and_log_empty_or_illegal_lines(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length){
    while(go_to_next_line(ps)){
        log_and_fail_if_file_line_cant_be_processed(ps, max_lines, max_parsable_lines, max_line_length);
        if(check_and_log_if_line_is_empty(ps)) return;
    }
}