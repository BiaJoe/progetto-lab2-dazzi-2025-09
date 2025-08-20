#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "errors.h"
#include "structs.h"

// macro per rendere il codice più breve e leggibile



#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MANHATTAN(x1,y1,x2,y2) (ABS((x1) - (x2)) + ABS((y1) - (y2)))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define TRUNCATE_STRING_AT_MAX_LENGTH(s,maxlen) \
	do { \
		if(strlen(s) >= (maxlen)) \
			(s)[(maxlen)-1] = '\0'; \
	} while(0)

// macros per fare la fork di processi
// metto due __underscore così non rischio di usare le variabili per sbaglio
#define FORK_PROCESS(child_function_name, parent_function_name) \
	do { 																													\
		pid_t __fork_pid = fork(); 																	\
		check_error_fork(__fork_pid); 															\
		if (__fork_pid == 0) 																				\
			child_function_name(); 																		\
		else																												\
			parent_function_name(); 																	\
	} while(0) 

#define LOCK(m) mtx_lock(&(m))
#define UNLOCK(m) mtx_unlock(&(m))

// funzioni di utilità generale
int is_line_empty(char *line);
int my_atoi(char a[]);

// funzioni di accesso a strutture
rescuer_type_t * get_rescuer_type_by_name(char *name, rescuer_type_t **rescuer_types);
emergency_type_t * get_emergency_type_by_name(char *name, emergency_type_t **emergency_types);
rescuer_request_t * get_rescuer_request_by_name(char *name, rescuer_request_t **rescuers);
char* get_name_of_rescuer_requested(rescuer_request_t *rescuer_request);
int get_time_before_emergency_timeout_from_priority(int p);
rescuer_type_t *get_rescuer_type_by_index(server_context_t *ctx, int i);
rescuer_digital_twin_t *get_rescuer_digital_twin_by_index(rescuer_type_t *r, int rescuer_digital_twin_index);

// funzioni per liberare strutture allocate dinamicamente che non sono ad uso specifico del processo in cui sono allocate
// esempio: i rescuer types sono allocati dal parser ma liberati dal server
void free_rescuer_types(rescuer_type_t **rescuer_types);
void free_emergency_types(emergency_type_t **emergency_types);
void free_rescuer_requests(rescuer_request_t **rescuer_requests);


#endif