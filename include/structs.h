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
	bool is_travelling;
	bool has_arrived;
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


#endif