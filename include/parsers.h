#ifndef PARSERS_H
#define PARSERS_H

#define _GNU_SOURCE // per usare getline

#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>
#include "structs.h"
#include "log.h"
#include "bresenham.h"

#define MAX_RESCUER_CONF_LINES 128
#define MAX_RESCUER_CONF_LINE_LENGTH 512
#define MAX_EMERGENCY_CONF_LINES 256
#define MAX_EMERGENCY_CONF_LINE_LENGTH 512
#define MAX_ENV_CONF_LINES 12
#define MAX_ENV_CONF_LINE_LENGTH 128

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
	char emergency_desc[EMERGENCY_NAME_LENGTH];											
	char rescuer_requests_string[MAX_RESCUER_REQUESTS_LENGTH + 1];	
	int rescuers_req_number;	
	rescuer_request_t **rescuers; 		
} emergency_type_fields_t;

typedef struct {
	char queue_name[MAX_EMERGENCY_QUEUE_NAME_LENGTH + 1];
	int height;
	int width;
} environment_t;

typedef struct {
	rescuer_type_t **types;
	int count;
} rescuers_t;

typedef struct {
	emergency_type_t **types;
	int count;
} emergencies_t;

// funzioni generali
environment_t *parse_env();
rescuers_t *parse_rescuers(int x, int y);
emergencies_t *parse_emergencies(rescuers_t *rescuers);
bool go_to_next_line(parsing_state_t *ps);

// parsing utils
bool check_and_log_if_line_is_empty(parsing_state_t *ps);
void log_and_fail_if_file_line_cant_be_processed(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length);
void skip_and_log_empty_or_illegal_lines(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length);

// funzioni di checking errori ed estrazione valori
bool rescuer_type_values_are_illegal(rescuer_type_fields_t *fields);
bool rescuer_request_values_are_illegal(char *name, int required_count, int time_to_manage);
bool emergency_values_are_illegal(emergency_type_fields_t *fields);
bool environment_values_are_illegal(environment_t *e);

// rescuers
int check_and_extract_rescuer_type_fields_from_line(parsing_state_t *ps, rescuer_type_t **rescuer_types, rescuer_type_fields_t *fields);
int check_and_log_if_rescuer_type_already_parsed(parsing_state_t *ps, rescuer_type_t **types, char* name);

// emergenze
bool check_and_log_if_rescuer_request_already_parsed(parsing_state_t *ps, rescuer_request_t **requests, char* name);
bool check_and_log_if_emergency_type_already_parsed(parsing_state_t *ps, emergency_type_t**types, emergency_type_fields_t* fields);
void check_if_rescuer_requested_is_available(parsing_state_t *ps, rescuer_type_t **types, char *name, int count);
bool check_and_extract_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_t **emergency_types, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields);
void check_and_extract_simple_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_fields_t *fields);
void check_and_extract_rescuer_requests_from_string(parsing_state_t *ps, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields);
void check_and_extract_rescuer_request_fields_from_token(parsing_state_t *ps, int requests_parsed_so_far, char*token, char *name, int *count, int *time);

// ambiente
void check_and_extract_env_line_field(parsing_state_t *ps, environment_t *e, const char *syntax);

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