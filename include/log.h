#ifndef LOG_H
#define LOG_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif 

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <threads.h>
#include <unistd.h>

#include "utils.h"

#define LOG_INIT_SERVER() log_init(1)
#define LOG_INIT_CLIENT() log_init(0)

#define LOG_SENDER_BUFFER_CAPACITY 128
#define HOW_MANY_LOG_MESSAGES_TO_SEND_AT_A_TIME 4
#define NANOSECONDS_BETWEEN_EACH_LOG_SENDING 10000000

#define check_opened_file_error_log(fp) \
	do { \
		if(!fp) \
			log_fatal_error("errore apertura file"); \
	}while(0);

// una macro per standardizzare il formato degli errori in linee di file
#define LINE_FILE_ERROR_STRING "Errore a linea %d in file %s: "
 
// log.c serve per inviare messaggi di log a logger.c, che li scriverà su file
extern mqd_t log_mq;
void log_close();
void log_init(int unlink);
int log_sender_thread();
void enqueue_log_message(char buffer[], long long time);
void log_event(int id, log_event_type_t event_type, char *message, ...);
void log_fatal_error(char *format, ...);

long get_time();
log_event_info_t* get_log_event_info(log_event_type_t event_type);
char* get_log_event_type_string(log_event_type_t event_type);
char* get_log_event_type_code(log_event_type_t event_type);
void increment_log_event_type_counter(log_event_type_t event_type);
int get_log_event_type_counter(log_event_type_t event_type);
int is_log_event_type_terminating(log_event_type_t event_type);
int is_log_event_type_to_log(log_event_type_t event_type);


#endif