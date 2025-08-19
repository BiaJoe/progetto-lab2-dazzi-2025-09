#ifndef STRUCTS_H
#define STRUCTS_H

#include <time.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "constants.h"

// STRUTTURA HELPER PER IL PARSING

typedef struct {
	FILE *fp;
	char* filename;			
	char* line;						// la linea che stiamo parsando (init a NULL perchè sarà analizzata da getline())
	size_t len;						// per getline()
	int 	line_number;		// il suo numero		
	int 	parsed_so_far;	// quante entità abbiamo raccolto fin ora
}	parsing_state_t;


// STRUTTURE PER IL LOGGING

typedef struct {
	char name[LOG_EVENT_NAME_LENGTH];	// versione stringa del tipo
	char code[LOG_EVENT_CODE_LENGTH];	// versione codice del tipo
	atomic_int counter;								// quante volte è stato registrato
	int is_terminating;								// se loggarlo vuol dire terminare il programma		
	int is_to_log;										// se va scritto o no nel file di log
} log_event_info_t;

 
typedef struct {
	long long timestamp;
	char message[MAX_LOG_EVENT_LENGTH];
} log_message_t;

#define LOG_EVENT_TYPES_COUNT 34

typedef enum {

	NON_APPLICABLE, 								  //N/A
	DEBUG, 
	// errori fatali
	FATAL_ERROR, 											//FERR
	FATAL_ERROR_PARSING, 							//FEPA
	FATAL_ERROR_LOGGING, 							//FELO
	FATAL_ERROR_MEMORY, 							//FEME
	FATAL_ERROR_FILE_OPENING,					//FEFO
	
	// errori non fatali
	EMPTY_CONF_LINE_IGNORED,					//ECLI		
	DUPLICATE_RESCUER_REQUEST_IGNORED,//DRRI
	WRONG_RESCUER_REQUEST_IGNORED, 		//WRRI
	DUPLICATE_EMERGENCY_TYPE_IGNORED, //DETI
	DUPLICATE_RESCUER_TYPE_IGNORED,		//DRTI
	WRONG_EMERGENCY_REQUEST_IGNORED_CLIENT, //WERC
	WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, //WERS

	// eventi di log
	LOGGING_STARTED, 									//LSTA
	LOGGING_ENDED,  									//LEND

	// eventi di parsing
	PARSING_STARTED,									//PSTA
	PARSING_ENDED,										//PEND
	RESCUER_TYPE_PARSED,							//RTPA
	RESCUER_DIGITAL_TWIN_ADDED,				//RDTA
	EMERGENCY_PARSED,									//EMPA		
	RESCUER_REQUEST_ADDED,						//RRAD

	SERVER_UPDATE, // seup
	SERVER, //srvr
	CLIENT, //clnt

	// eventi di gestione richieste emergenza
	EMERGENCY_REQUEST_RECEIVED, 			//ERRR
	EMERGENCY_REQUEST_PROCESSED,			//ERPR

	MESSAGE_QUEUE_CLIENT, 						//MQCL
	MESSAGE_QUEUE_SERVER, 						//MQSE

	EMERGENCY_STATUS, 								//ESTA
	RESCUER_STATUS, 									//RSTA
	RESCUER_TRAVELLING_STATUS,         //RTST
	EMERGENCY_REQUEST,								//ERRE

	PROGRAM_ENDED_SUCCESSFULLY,				//PESU

	// ...aggiungere altri tipi di log qui
} log_event_type_t; 


// STRUTTURE PER I RESCUER

typedef enum {
	IDLE,
	EN_ROUTE_TO_SCENE,
	ON_SCENE,
	RETURNING_TO_BASE
} rescuer_status_t;

typedef struct {	// struttura per la computazione del percorso di ogni rescuer
	int x_target;
	int y_target;
	int dx;					// distanze tra partenza e arrivo in x e y
	int dy;
	int sx;					// direzioni: x e y aumentano o diminuiscono?
	int sy;
	int err;				// l'errore di Bresenham: permette di decidere se muoversi sulla x o sulla y
} bresenham_trajectory_t;

// Foreward declaration per rescuer_digital_twin

typedef struct emergency_node emergency_node_t;
typedef struct rescuer_digital_twin rescuer_digital_twin_t;

typedef struct {
	char *rescuer_type_name;
	int amount;
	int speed;
	int x;
	int y;
	rescuer_digital_twin_t **twins;
} rescuer_type_t;

struct rescuer_digital_twin {
	int id;
	int x;
	int y;
	rescuer_type_t *rescuer;
	rescuer_status_t status;
	bresenham_trajectory_t *trajectory; // serve per tenere traccia di dove il gemello sta andando e del percorso che fa 
	emergency_node_t *emergency_node;		// ogni gemello è legato all'emergenza che sta gestendo
	int is_travelling;
	int has_arrived;
	int time_to_manage;
	int time_left_before_it_can_leave_the_scene;
};

// STRUTTURE PER LE EMERGENZE

typedef struct {
	rescuer_type_t *type;
	int required_count;
	int time_to_manage;
} rescuer_request_t;

typedef struct {
	short priority;
	char *emergency_desc;
	rescuer_request_t **rescuers;
	int rescuers_req_number;
} emergency_type_t;

typedef enum {
	WAITING,
	ASSIGNED,
	ASKING_FOR_RESCUERS,
	WAITING_FOR_RESCUERS,
	IN_PROGRESS,
	PAUSED,
	COMPLETED,
	CANCELED,
	TIMEOUT
} emergency_status_t;


// STRUTTURE PER LE RICHIESTE DI EMERGENZA

typedef struct {
	char emergency_name[EMERGENCY_NAME_LENGTH];
	int x;
	int y;
	time_t timestamp;
} emergency_request_t;


typedef struct {
	emergency_type_t *type;
	int id;
	int priority;
	emergency_status_t status;
	int x;
	int y;
	time_t time;
	int rescuer_count;
	rescuer_digital_twin_t **rescuer_twins;
	int time_since_started_waiting;
	int time_since_it_was_assigned;
	int time_spent_existing;
} emergency_t;

// emergency queue

// forward declaration per node e list
typedef struct emergency_list emergency_list_t;

#define UNDEFINED_TIME_FOR_RESCUERS_TO_ARRIVE -1

struct emergency_list {
	emergency_node_t *head;
	emergency_node_t *tail;
	int node_amount; 
	mtx_t mutex;
};

struct emergency_node {
	emergency_list_t *list;
	int rescuers_found;
	int rescuers_are_arriving;
	int rescuers_have_arrived;
	int time_estimated_for_rescuers_to_arrive;
	emergency_t *emergency;
	struct emergency_node *prev;
	struct emergency_node *next;
	mtx_t mutex;			
	cnd_t waiting;
};

typedef struct {
	emergency_list_t* lists[PRIORITY_LEVELS]; 
	int is_empty;
	cnd_t not_empty;									// condizione per notificare che la coda non è vuota
	mtx_t mutex; 
} emergency_queue_t;

// server

typedef struct {
	int height;																// interi per tenere traccia di cosa succede
	int width;
	int rescuer_types_count;
	int emergency_types_count;
	int emergency_requests_count;
	int valid_emergency_request_count;
	rescuer_type_t** rescuer_types;						// puntatori alle strutture rescuers
	mtx_t rescuers_mutex;											// mutex per proteggere l'accesso ai rescuer types
	emergency_type_t** emergency_types;				// puntatori alle strutture emergency_types
	emergency_queue_t* waiting_queue;					// coda per contenere le emergenze da processare
	emergency_queue_t* working_queue;					// coda per contenere le emergenze assegnate a un thread
	emergency_list_t* completed_emergencies;	// lista per tenere traccia di tutte le emergenze completate
	emergency_list_t* canceled_emergencies; 	// lista delle emergenze cancellate
	mqd_t mq;																	// message queue per ricevere le emergenze dai client	
	int tick;																	// tick del server, per sincronizzare i thread
	int tick_count_since_start;								// contatore dei tick del server, per tenere traccia di quanti tick sono stati fatti
	mtx_t clock_mutex;												// mutex per proteggere l'accesso al tick del server
	cnd_t clock_updated;											// condizione per comunicare al therad updater di fare l'update
	atomic_bool server_must_stop;							// flag che segnala ai thread di fermarsi
} server_context_t;



#endif