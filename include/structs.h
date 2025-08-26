#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdbool.h>
#include <time.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <semaphore.h>
#include <string.h>

#include "bresenham.h"

#define MAX_EMERGENCY_QUEUE_NAME_LENGTH 32
#define MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH 512

#define STOP_MESSAGE_FROM_CLIENT "-stop"

#define MAX_EMERGENCY_NAME_LENGTH 64


#define STR_HELPER(x) #x    
#define STR(x) STR_HELPER(x)

#define NO_TIMEOUT   -1
#define NO_PROMOTION -1
#define INVALID_TIME -1

// sgtruttura per la priorità delle emergenze

typedef struct {
    short number;
    int seconds_before_promotion; 
    int time_before_timeout;
} priority_rule_t;

// RICHIESTE DI EMERGENZA

typedef struct {
	char emergency_name[MAX_EMERGENCY_NAME_LENGTH];
	int x;
	int y;
	time_t timestamp;
} emergency_request_t;

// STRUTTURE PER I RESCUER

typedef enum {
	IDLE,
	EN_ROUTE_TO_SCENE,
	ON_SCENE,
	RETURNING_TO_BASE
} rescuer_status_t;


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
	bresenham_trajectory_t *trajectory; 	// serve per tenere traccia di dove il gemello sta andando e del percorso che fa 
	emergency_node_t *emergency_node;		// ogni gemello è legato all'emergenza che sta gestendo
	bool is_travelling;
	bool has_arrived;
	int time_to_manage;
	int time_left_before_leaving;
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


// Strutture per contenere i dati parsati dai file di configurazione

typedef struct {
	char queue_name[MAX_EMERGENCY_QUEUE_NAME_LENGTH];
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

// struttura per shared memory: comunicazione client server del nome della coda

#define SHM_NAME "/emergency_shm"
#define SEM_NAME "/emergency_shm_ready" 

typedef struct {
    char queue_name[MAX_EMERGENCY_QUEUE_NAME_LENGTH];
	char requests_argument_separator;
} client_server_shm_t;

// funzioni di accesso a strutture
rescuer_type_t * get_rescuer_type_by_name(char *name, rescuer_type_t **rescuer_types);
emergency_type_t * get_emergency_type_by_name(char *name, emergency_type_t **emergency_types);
rescuer_request_t * get_rescuer_request_by_name(char *name, rescuer_request_t **rescuers);
char* get_name_of_rescuer_requested(rescuer_request_t *rescuer_request);

// funzioni per liberare strutture allocate dinamicamente che non sono ad uso specifico del processo in cui sono allocate
// esempio: i rescuer types sono allocati dal parser ma liberati dal server
void free_rescuer_types(rescuer_type_t **rescuer_types);
void free_emergency_types(emergency_type_t **emergency_types);
void free_rescuer_requests(rescuer_request_t **rescuer_requests);

#endif