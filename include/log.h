#ifndef LOG_H
#define LOG_H

#include "errors.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <mqueue.h>
#include <errno.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <threads.h>
#include <unistd.h>
#include <stdatomic.h>

#define LOG_EVENT_TOTAL_LENGTH 512
#define LOG_EVENT_NAME_LENGTH 64
#define LOG_EVENT_CODE_LENGTH 5

#define LOG_QUEUE_NAME 						   "/log_queue"
#define MAX_LOG_QUEUE_MESSAGES 				   10
#define LOG_QUEUE_OPEN_MAX_RETRIES             128    
#define LOG_QUEUE_OPEN_RETRY_INTERVAL_NS	   5000000   // 5 ms

#define MAX_LOG_EVENT_TIMESTAMP_STRING_LENGTH  32
#define MAX_LOG_EVENT_ID_STRING_LENGTH         16
#define MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH 300

// ID speciali
enum { AUTOMATIC_LOG_ID = -1, NON_APPLICABLE_LOG_ID = -2 };

typedef struct {
	char name[LOG_EVENT_NAME_LENGTH];	// versione stringa del tipo
	char code[LOG_EVENT_CODE_LENGTH];	// versione codice del tipo
	bool is_terminating;					// se loggarlo vuol dire terminare il programma		
	bool is_to_log;						// se va scritto o no nel file di log
} log_event_info_t;


typedef struct logging_config {
    char *log_file;
    FILE *log_file_ptr;
    char *logging_syntax;
    char *non_applicable_log_id_string;
    bool log_to_file;
    bool log_to_stdout;
    int  flush_every_n;
} logging_config_t;

// una macro per standardizzare il formato degli errori in linee di file
#define LINE_FILE_ERROR_STRING "Errore a linea %d in file %s: "

// STRUTTURE PER IL LOGGING

typedef enum {
    NON_APPLICABLE = 0,
    DEBUG,

    // errori fatali 
    FATAL_ERROR,
	FATAL_ERROR_CLIENT,
    FATAL_ERROR_PARSING,		
    FATAL_ERROR_LOGGING,
    FATAL_ERROR_MEMORY,
    FATAL_ERROR_FILE_OPENING,
    
    // errori non fatali
    PARSING_ERROR,
    EMPTY_CONF_LINE_IGNORED,
    DUPLICATE_RESCUER_REQUEST_IGNORED,
    WRONG_RESCUER_REQUEST_IGNORED,
    DUPLICATE_EMERGENCY_TYPE_IGNORED,
    DUPLICATE_RESCUER_TYPE_IGNORED,
    WRONG_EMERGENCY_REQUEST_IGNORED_CLIENT,
    WRONG_EMERGENCY_REQUEST_IGNORED_SERVER,

    // eventi di log
    LOGGING_STARTED,
    LOGGING_ENDED,

    // eventi di parsing
    PARSING_STARTED,
    PARSING_ENDED,
    RESCUER_TYPE_PARSED,
    RESCUER_DIGITAL_TWIN_ADDED,
    EMERGENCY_PARSED,
    RESCUER_REQUEST_ADDED,

    SERVER_UPDATE,
    SERVER,
    CLIENT,

    // eventi di gestione richieste emergenza
    EMERGENCY_REQUEST_RECEIVED,
    EMERGENCY_REQUEST_PROCESSED,

    MESSAGE_QUEUE_CLIENT,
    MESSAGE_QUEUE_SERVER,

    EMERGENCY_STATUS,
    RESCUER_STATUS,
    RESCUER_TRAVELLING_STATUS,
    EMERGENCY_REQUEST,

    PROGRAM_ENDED_SUCCESSFULLY,

    // ...aggiungere altri tipi di log qui

    LOG_EVENT_TYPES_COUNT // deve essere l'ultimo
} log_event_type_t;

typedef enum { 
	LOG_ROLE_SERVER = 1, // consuma e produce messaggi di log
	LOG_ROLE_CLIENT = 0  // produce messaggi di log
} log_role_t;

typedef struct {
    log_role_t role; 
    char timestamp[MAX_LOG_EVENT_TIMESTAMP_STRING_LENGTH];
    int id;
    log_event_type_t event_type;            
    char formatted_message[MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH];            
    char text[LOG_EVENT_TOTAL_LENGTH];  // SOLO il testo, senza prefisso
} log_message_t;


int  log_init(log_role_t role, const logging_config_t config);
void log_close(void);  

void assemble_log_text(char destination_string[LOG_EVENT_TOTAL_LENGTH], log_message_t m);
void log_event(int id, log_event_type_t event_type, char *format, ...);
void log_error_and_exit(void (*exit_function)(int), const char *format, ...);
void log_fatal_error(char *format, ...);
void log_parsing_error(char *format, ...);


const log_event_info_t* get_log_event_info(log_event_type_t event_type);


#endif 

