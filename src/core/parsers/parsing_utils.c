#include "parsers.h"

parsing_state_t *mallocate_parsing_state(char *filename){
	parsing_state_t *ps = (parsing_state_t *)malloc(sizeof(parsing_state_t));
	check_error_memory_allocation(ps);
	ps->fp = fopen(filename, "r");					
	check_error_fopen(ps->fp);	
	ps->filename = strdup(filename); // alloco e copio il filename
  check_error_memory_allocation(ps->filename);
	ps->line = NULL; 
	ps->len = 0;
	ps->line_number = 0;
	ps->parsed_so_far = 0;
	ps->should_continue_parsing = true;
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

bool line_is_empty_logging(parsing_state_t *ps){
	if (is_line_empty(ps->line)){
		log_event(AUTOMATIC_LOG_ID, EMPTY_CONF_LINE_IGNORED, "linea vuota ignorata (%d) %s", ps->line_number, ps->filename);
		return true;
	}
	return false;
}

static bool line_is_comment(char *line) {
	if (!line) return false;
    char *s = line;
    while (*s == ' ' || *s == '\t') s++;

    if (*s == COMMENT_CHAR) return true;
	return false;
}

// logga e ritorna false se la riga analizzata è illegale secondo i criteri passati da input
bool check_if_line_can_be_processed_logging(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length){
	if(ps->line_number > max_lines) { 				
		ps->should_continue_parsing = false; 
		log_parsing_error("Numero massimo di linee superato in %s", ps->filename);
		return false;
	}

	if(ps->parsed_so_far > max_parsable_lines) {
		ps->should_continue_parsing = false; 
		log_parsing_error("Numero massimo di linee elaborate superato in %s", ps->filename);
		return false;
	}

	if((int) strlen(ps->line) > max_line_length) {
		ps->should_continue_parsing = false; 
		log_parsing_error(LINE_FILE_ERROR_STRING "Linea troppo lunga", ps->line_number, ps->filename);
		return false;
	}

	if (line_is_comment(ps->line)){
		log_event(AUTOMATIC_LOG_ID, EMPTY_CONF_LINE_IGNORED, "linea commentata ignorata (%d) %s", ps->line_number, ps->filename);
		return false;
	}

	if (is_line_empty(ps->line)){
		log_event(AUTOMATIC_LOG_ID, EMPTY_CONF_LINE_IGNORED, "linea vuota ignorata (%d) %s", ps->line_number, ps->filename);
		return false;
	}

	return true;
}

// salta le linee vuote o commentate 
// ritorna false se trova una linea illegale, altrimenti true
bool skip_and_log_empty_or_illegal_lines(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length){
    while (go_to_next_line(ps)) {
        if (!check_if_line_can_be_processed_logging(ps, max_lines, max_parsable_lines, max_line_length))
			continue;
        // se arrivo qui, la riga è processabile
        return true;
    }
    return false;
}