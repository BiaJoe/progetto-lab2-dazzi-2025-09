#ifndef PARSERS_H
#define PARSERS_H

#define _GNU_SOURCE // per usare getline

#include <stdlib.h>
#include <threads.h>
#include "structs.h"
#include "log.h"
#include "bresenham.h"

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
	char queue_name[EMERGENCY_QUEUE_NAME_LENGTH + 1];
	int height;
	int width;
} environment_fields_t;

// funzioni generali
void parse_rescuers(server_context_t *ctx);
void parse_emergencies(server_context_t *ctx);
void parse_env(server_context_t *ctx);
int go_to_next_line(parsing_state_t *ps);

// parsing utils
int check_and_log_if_line_is_empty(parsing_state_t *ps);
void log_and_fail_if_file_line_cant_be_processed(parsing_state_t *ps, int max_lines, int max_parsable_lines, int max_line_length);

// funzioni di checking errori ed estrazione valori
int rescuer_type_values_are_illegal(rescuer_type_fields_t *fields);
int rescuer_request_values_are_illegal(char *name, int required_count, int time_to_manage);
int emergency_values_are_illegal(emergency_type_fields_t *fields);
int environment_values_are_illegal(environment_fields_t *e);

// rescuers
int check_and_extract_rescuer_type_fields_from_line(parsing_state_t *ps, rescuer_type_t **rescuer_types, rescuer_type_fields_t *fields);
int check_and_log_if_rescuer_type_already_parsed(parsing_state_t *ps, rescuer_type_t **types, char* name);

// emergenze
int check_and_log_if_emergency_type_already_parsed(parsing_state_t *ps, emergency_type_t**types, emergency_type_fields_t* fields);
int check_and_log_if_rescuer_request_already_parsed(parsing_state_t *ps, rescuer_request_t **requests, char* name);
void check_if_rescuer_requested_is_available(parsing_state_t *ps, rescuer_type_t **types, char *name, int count);
int check_and_extract_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_t **emergency_types, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields);
void check_and_extract_simple_emergency_type_fields_from_line(parsing_state_t *ps, emergency_type_fields_t *fields);
void check_and_extract_rescuer_requests_from_string(parsing_state_t *ps, rescuer_type_t **rescuer_types, emergency_type_fields_t *fields);
void check_and_extract_rescuer_request_fields_from_token(parsing_state_t *ps, int requests_parsed_so_far, char*token, char *name, int *count, int *time);

// ambiente
void check_and_extract_env_line_field(parsing_state_t *ps, environment_fields_t *e);

// memoria
parsing_state_t *mallocate_parsing_state(char *filename);
void free_parsing_state(parsing_state_t *ps);

rescuer_type_t **callocate_rescuer_types();
rescuer_type_t *mallocate_and_populate_rescuer_type(rescuer_type_fields_t *fields);
rescuer_digital_twin_t **callocate_and_populate_rescuer_digital_twins(rescuer_type_t* r);
rescuer_digital_twin_t *mallocate_rescuer_digital_twin(rescuer_type_t* r);
rescuer_digital_twin_t *mallocate_rescuer_digital_twin(rescuer_type_t* r);

emergency_type_t 	**callocate_emergency_types();
rescuer_request_t **callocate_rescuer_requests();
emergency_type_t *mallocate_and_populate_emergency_type(emergency_type_fields_t *fields);
rescuer_request_t *mallocate_and_populate_rescuer_request(int required_count, int time_to_manage, rescuer_type_t *type);

#endif