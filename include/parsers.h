#ifndef PARSERS_H
#define PARSERS_H

#define _GNU_SOURCE // per usare getline

#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>
#include "structs.h"
#include "log.h"
#include "bresenham.h"

#define COMMENT_CHAR '#'

#define MIN_X_COORDINATE_ABSOLUTE_VALUE 0
#define MAX_X_COORDINATE_ABSOLUTE_VALUE 1024
#define MIN_Y_COORDINATE_ABSOLUTE_VALUE 0
#define MAX_Y_COORDINATE_ABSOLUTE_VALUE 1024

#define MIN_RESCUER_SPEED 1
#define MAX_RESCUER_SPEED 100
#define MIN_RESCUER_AMOUNT 1
#define MAX_RESCUER_AMOUNT 1000
#define MIN_RESCUER_REQUIRED_COUNT 1
#define MAX_RESCUER_REQUIRED_COUNT 32
#define MIN_TIME_TO_MANAGE 1
#define MAX_TIME_TO_MANAGE 1000

#define MAX_RESCUER_NAME_LENGTH 128
#define MAX_RESCUER_REQUESTS_LENGTH 1024
#define MAX_RESCUER_TYPES_COUNT 64
#define MAX_EMERGENCY_TYPES_COUNT 128
#define ENV_CONF_VALID_LINES_COUNT 3
#define MAX_RESCUER_REQ_NUMBER_PER_EMERGENCY 16
#define MAX_ENV_FIELD_LENGTH 32

#define RESCUERS_SYNTAX "[%" STR(MAX_RESCUER_NAME_LENGTH) "[^]]][%d][%d][%d;%d]"
#define RESCUER_REQUEST_SYNTAX "%" STR(MAX_RESCUER_NAME_LENGTH) "[^:]:%d,%d"
#define EMERGENCY_TYPE_SYNTAX "[%" STR(MAX_EMERGENCY_NAME_LENGTH) "[^]]] [%hd] %" STR(MAX_RESCUER_REQUESTS_LENGTH) "[^\n]"
#define RESCUER_REQUESTS_SEPARATOR ";"

// Accetta: "queue=NAME", "queue = NAME", "queue    NAME" (senza =), ecc.
#define ENV_CONF_QUEUE_SYNTAX  "queue%*[ \t=]%" STR(MAX_EMERGENCY_QUEUE_NAME_LENGTH) "s"
#define ENV_CONF_HEIGHT_SYNTAX "height%*[ \t=]%d"
#define ENV_CONF_WIDTH_SYNTAX  "width%*[ \t=]%d"

#define EMERGENCY_REQUEST_SYNTAX "%" STR(MAX_EMERGENCY_NAME_LENGTH) "[^0-9] %d %d %d"

// questa sintassi ammette numeri nel nome ma non spazi 
// #define CLIENT_REQUEST_SYNTAX "%" STR(MAX_EMERGENCY_NAME_LENGTH) "s %d %d %d"

// STRUTTURA HELPER PER IL PARSING

typedef struct {
	FILE   *fp;
	char   *filename;			
	char   *line;			// la linea che stiamo parsando (init a NULL perchè sarà analizzata da getline())
	size_t 	len;			// per getline()
	int 	line_number;	// il suo numero		
	int 	parsed_so_far;	// quante entità abbiamo raccolto fin ora
	bool 	should_continue_parsing;
} parsing_state_t;

typedef struct {
	char name[MAX_RESCUER_NAME_LENGTH];
	int amount;
	int speed;
	int x;
	int y;
	int max_x_coordinate;
	int max_y_coordinate;
} rescuer_type_fields_t;

typedef struct {
	short priority;																								
	char emergency_desc[MAX_EMERGENCY_NAME_LENGTH];											
	char rescuer_requests_string[MAX_RESCUER_REQUESTS_LENGTH + 1];	
	int rescuers_req_number;	
	rescuer_request_t **rescuers; 		
} emergency_type_fields_t;

// funzioni generali
environment_t* parse_env(char *filename);
rescuers_t*    parse_rescuers(char *filename, int x, int y);
emergencies_t* parse_emergencies(char *filename, rescuers_t *rescuers, const priority_rule_t* priority_LUT, int priority_count);

// parsing utils
bool line_is_empty_logging(parsing_state_t *ps);
bool check_if_line_can_be_processed_logging(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length);
bool skip_and_log_empty_or_illegal_lines(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length);
bool go_to_next_line(parsing_state_t *ps);

// funzioni di checking errori ed estrazione valori
bool rescuer_type_values_are_illegal(rescuer_type_fields_t *fields);
bool rescuer_request_values_are_illegal(char *name, int required_count, int time_to_manage);
bool emergency_values_are_illegal(emergency_type_fields_t *fields);
bool environment_values_are_illegal(environment_t *e);

// rescuers
bool check_and_extract_rescuer_type_fields_from_line(parsing_state_t *ps, rescuer_type_t **rescuer_types, rescuer_type_fields_t *fields);
bool check_and_log_if_rescuer_type_already_parsed(parsing_state_t *ps, rescuer_type_t **types, char* name);

// emergenze & richieste di emergenza
bool check_and_log_if_rescuer_request_already_parsed(parsing_state_t *ps, rescuer_request_t **requests, char* name);
bool check_and_log_if_emergency_type_already_parsed(parsing_state_t *ps, emergency_type_t**types, emergency_type_fields_t* fields);
bool check_if_rescuer_requested_is_available(parsing_state_t *ps, rescuer_type_t **types, char *name, int count);
bool check_and_extract_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_t **emergency_types, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields);
bool check_and_extract_simple_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_fields_t *fields);
bool check_and_extract_rescuer_requests_from_string(parsing_state_t *ps, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields);
bool check_and_extract_rescuer_request_fields_from_token(parsing_state_t *ps, int requests_parsed_so_far, char*token, char *name, int *count, int *time);

emergency_request_t *parse_emergency_request(char *message, emergency_type_t **types, int envh, int envw);
bool emergency_request_values_are_illegal(emergency_type_t **types, int envh, int envw, char* name, int x, int y);


// ambiente
void check_and_extract_env_line_field(parsing_state_t *ps, void *variable_ptr, const char *syntax);
void cleanup_queue_name(char* qn);

// memoria
parsing_state_t *mallocate_parsing_state(char *filename);
void free_parsing_state(parsing_state_t *ps);
environment_t *mallocate_enviroment();
rescuer_type_t **callocate_rescuer_types();
rescuer_type_t *mallocate_and_populate_rescuer_type(rescuer_type_fields_t *fields);
rescuer_digital_twin_t **callocate_and_populate_rescuer_digital_twins(rescuer_type_t* r);
rescuer_digital_twin_t *mallocate_rescuer_digital_twin(rescuer_type_t* r);
rescuer_digital_twin_t *mallocate_rescuer_digital_twin(rescuer_type_t* r);

emergency_type_t **callocate_emergency_types();
rescuer_request_t **callocate_rescuer_requests();
emergency_type_t *mallocate_and_populate_emergency_type(emergency_type_fields_t *fields);
rescuer_request_t *mallocate_and_populate_rescuer_request(int required_count, int time_to_manage, rescuer_type_t *type);

#endif