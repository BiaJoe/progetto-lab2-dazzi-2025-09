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

#include "structs.h"

// macro per rendere il codice più breve e leggibile

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MANHATTAN(x1,y1,x2,y2) (ABS((x1) - (x2)) + ABS((y1) - (y2)))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define I_HAVE_TO_CLOSE_THE_LOG(m) ((strcmp(m, STOP_LOGGING_MESSAGE) == 0) ? 1 : 0 )
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

//error checking macros (le uniche macro del progetto che non sono in all caps)
 
#define check_error(cond, msg)                            \
  do {                                                    \
    if (cond) {                                           \
      fprintf(stderr, "\n[ERROR at %s: %d] ", __FILE__, __LINE__); \
			perror(msg); \
      exit(EXIT_FAILURE);                                 \
    }                                                     \
  } while (0)
	
#define check_error_fopen(fp) 						check_error((fp) == NULL, "fopen")
#define check_error_memory_allocation(p) 	check_error(!(p), "memory allocation")

#define check_error_mq_open(mq) 					check_error((mq) == (mqd_t)-1, "mq_open") 
#define check_error_mq_send(i) 						check_error((i) == -1, "mq_send")
#define check_error_mq_receive(b) 				check_error((b) == -1, "mq_receive")
#define try_to_open_queue(mq, queue_name, max_attempts, nanoseconds_interval) 		\
	do {																																\
		if ((mq) == (mqd_t)-1) { 																					\
			int attempts = 0; 																							\
			while (attempts < (max_attempts)) { 														\
				mq = mq_open(queue_name, O_WRONLY); 													\
				if (mq != (mqd_t)-1) 																					\
					break; 																											\
				struct timespec req = {																				\
						.tv_sec = 0,																							\
						.tv_nsec = (nanoseconds_interval)													\
				};																														\
				nanosleep(&req, NULL);																				\
				attempts++; 																									\
			} 																															\
			if (mq == (mqd_t)-1) {																					\
				perror("mq_open fallita dopo 10 tentativi"); 									\
				exit(EXIT_FAILURE); 																					\
			} 																															\
		}																																	\
	} while (0);



#define check_error_fork(pid) 						check_error((pid) < 0, "fork_failed")
#define check_error_syscall(call, m)			check_error((call) == -1, m)
#define check_error_mtx_init(call)  			check_error((call) != thrd_success, "mutex init")
#define check_error_cnd_init(call)  			check_error((call) != thrd_success, "cnd init")

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