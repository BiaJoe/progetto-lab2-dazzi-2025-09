#ifndef LOG_H
#define LOG_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
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

#include "errors.h"
#include "config.h"

#define LOG_QUEUE_NAME 						   "/log_queue"
#define MAX_LOG_QUEUE_MESSAGES 				   10
#define LOG_QUEUE_OPEN_MAX_RETRIES             128    
#define LOG_QUEUE_OPEN_RETRY_INTERVAL_NS	   5000000   // 5 ms

#define MAX_LOG_EVENT_MQ_MESSAGE_LENGTH        1024   
#define MAX__LOG_EVENT_TIMESTAMP_STRING_LENGTH 32
#define MAX_LOG_EVENT_ID_STRING_LENGTH         16
#define MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH 256
#define LOG_EVENT_NAME_LENGTH                  64
#define LOG_EVENT_CODE_LENGTH                  5

// ID speciali
enum { AUTOMATIC_LOG_ID = -1, NON_APPLICABLE_LOG_ID = -2 };

// una macro per standardizzare il formato degli errori in linee di file
#define LINE_FILE_ERROR_STRING "Errore a linea %d in file %s: "

// STRUTTURE PER IL LOGGING

typedef struct {
	char name[LOG_EVENT_NAME_LENGTH];	// versione stringa del tipo
	char code[LOG_EVENT_CODE_LENGTH];	// versione codice del tipo
	bool is_terminating;					// se loggarlo vuol dire terminare il programma		
	bool is_to_log;						// se va scritto o no nel file di log
} log_event_info_t;

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

typedef struct {
	int id;                       // id esplicito o AUTOMATIC_LOG_ID
    log_event_type_t event_type;                      
    char text[MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH];  // SOLO il testo, senza prefisso
} log_message_t;

/* Ruolo:
   - server: consuma dalla /log_queue e scrive su file/stdout (centralizzato)
   - client: solo produttore (invia in mqueue) */
typedef enum { 
	LOG_ROLE_SERVER = 1,
	LOG_ROLE_CLIENT = 0 
} log_role_t;

// API 🐝🐝🐝
int  log_init(log_role_t role);      // server crea sink; client apre writer 
void log_close(void);                // chiusura ordinata 

/* Funzione principale per loggare eventi:
   - formatta solo il testo del messaggio (printf-like)
   - invia (evt,id,testo) alla coda; prefisso lo aggiunge il server */
void log_event(int id, log_event_type_t event_type, char *format, ...);

// Helper per eventi fatali: logga come FATAL_ERROR e esce con errore 
void log_fatal_error(char *format, ...);

// Accessor LUT (diagnostica/debug) 
const log_event_info_t* get_log_event_info(log_event_type_t event_type);


#endif 

