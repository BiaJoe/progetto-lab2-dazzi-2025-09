#ifndef SERVER_H
#define SERVER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <signal.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "structs.h"
#include "priority_queue.h"
#include "log.h"
#include "parsers.h"

#define WORKER_THREADS_NUMBER 5

#define LOG_IGNORE_EMERGENCY_REQUEST(m) log_event(NO_ID, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, m)
#define LOG_EMERGENCY_REQUEST_RECIVED() log_event(NO_ID, EMERGENCY_REQUEST_RECEIVED, "emergenza ricevuta e messa in attesa di essere processata!")

// server

typedef struct {
	int count;
	int valid_count;
} requests_t;

typedef struct {
	emergency_t* array[WORKER_THREADS_NUMBER];
	atomic_int next_tw_index;
	mtx_t mutex;
} active_emergencies_array_t;

typedef struct server_clock {
	struct timespec tick_time;
	bool tick;								// tick del server, per sincronizzare i thread
	atomic_int tick_count_since_start;		// contatore dei tick del server, per tenere traccia di quanti tick sono stati fatti
	mtx_t mutex;							// mutex per proteggere l'accesso al tick del server
	cnd_t updated;							// condizione per comunicare al therad updater di fare l'update
} server_clock_t;

typedef struct {
	environment_t *enviroment;
	rescuers_t *rescuers;		    		// struttura che contiene i tipi di rescuer e altri interi utili
	emergencies_t *emergencies;	    		// struttura che contiene i tipi di emergenza
	requests_t *requests;           		// struttura che contiene il numero di richieste di emergenza totali e valide
	active_emergencies_array_t *active_emergencies;
	pq_t* emergency_queue;					// coda per contenere le emergenze da processare
	pq_t* completed_emergencies;			// lista per tenere traccia di tutte le emergenze completate
	pq_t* canceled_emergencies; 			// lista delle emergenze cancellate
	server_clock_t *clock;					// struttura per gestire il clock del server
	atomic_bool server_must_stop;			// flag che segnala ai thread di fermarsi
	mqd_t mq;								// message queue per ricevere le emergenze dai client	
    int shm_fd;                	 			// file descriptior della SHM
    client_server_shm_t *shm;    	
    sem_t *sem_ready;            	
} server_context_t;

// server_helpers.c

int compare_priorities(short a, short b);
void change_rescuer_digital_twin_destination_blocked(rescuer_digital_twin_t *t, int new_x, int new_y);
void remove_twin_from_its_emergency(rescuer_digital_twin_t *t);

void lock_emergencies();
void unlock_emergencies();
void lock_rescuers();
void unlock_rescuers();
void lock_system();
void unlock_system();

// main.c

void server_ipc_setup();
void close_server(int exit_code);
void init_server_context();
void cleanup_server_context();

int get_priority_level(short priority_number, const priority_rule_t *table, int priority_count);
int priority_to_level(short priority);
short level_to_priority(int level);
int priority_to_timeout_timer(short priority);
int priority_to_promotion_timer(short priority);
short get_next_priority(short p);

// thread_clock.c

int thread_clock(void *arg);
void tick();
void untick();
void wait_for_a_tick();
int server_is_ticking();
void lock_server_clock();
void unlock_server_clock();
int get_current_tick_count();

// thread_reciever.c

int thread_reciever(void *arg);
emergency_t *mallocate_emergency(int id, emergency_type_t **types, emergency_request_t *request);
void free_emergency(emergency_t* e);

// thread_worker.c

#define INDEX_NOT_FOUND -1
int thread_worker(void *arg);
void timeout_trash_close_logging_blocking(int i, emergency_t *e);
bool find_rescuers_logging_blocking(emergency_t *e);
void send_rescuer_digital_twin_to_scene_logging_blocked(rescuer_digital_twin_t *t, emergency_t *e);


// thread_updater.c
#define RESCUER_STATUS_FORMAT(t, message) \
	"%s #%d (%d, %d) v=%d " message, \
	t->rescuer->rescuer_type_name, \
	t->id, t->x, t->y, \
	t->rescuer->speed

#define EMERGENCY_STATUS_FORMAT(e, message) \
	"%s #%id (%d, %d) p=%d rna=%d rnd=%d tbt=%d tbp=%d " message, \
	e->type->emergency_desc, \
	e->id, e->x, e->y, e->priority, \
	e->rescuers_not_arrived_yet, \
	e->rescuers_not_done_yet, \
	e->timeout_timer, \
	e->promotion_timer

#define EMERGENCY_STATUS_SHORT_FORMAT(e, message) \
	"%s #%id (%d, %d) p=%d  " message, \
	e->type->emergency_desc, \
	e->id, e->x, e->y, e->priority


#define LOG_RESCUER_STATUS_DEBUG(t, message)					do { log_event(t->id, DEBUG, 			RESCUER_STATUS_FORMAT(t, message)); } while(0)
#define LOG_RESCUER_STATUS(t, message)  						do { log_event(t->id, RESCUER_STATUS, 	RESCUER_STATUS_FORMAT(t, message)); } while(0)
#define LOG_RESCUER_ARRIVED(t, message) 						do { log_event(t->id, RESCUER_STATUS, 			 "%s #%d -> (%d, %d) " message, t->rescuer->rescuer_type_name, t->id, t->x, t->y); } while(0)
#define LOG_RESCUER_MOVED(t, xA, yA)							do { log_event(t->id, RESCUER_TRAVELLING_STATUS, "%s #%d : (%d, %d) -> (%d, %d) il rescuer si è spostato", t->rescuer->rescuer_type_name, t->id, xA, yA, t->x, t->y); } while(0)
#define LOG_RESCUER_SENT(t, newx, newy, message)				do { log_event(t->id, RESCUER_STATUS, 			 "%s #%d : (%d, %d) .. (%d, %d) " message, t->rescuer->rescuer_type_name, t->id, t->x, t-> y, newx, newy); } while(0)
#define LOG_RESCUER_SENT_NAME(t, newx, newy, newname, message)	do { log_event(t->id, RESCUER_STATUS, 			 "%s #%d : (%d, %d) .. (%d, %d) %s" message, t->rescuer->rescuer_type_name, t->id, t->x, t-> y, newx, newy, newname); } while(0)


#define LOG_EMERGENCY_STATUS_DEBUG(e, message) 		do { log_event(e->id, DEBUG, 			EMERGENCY_STATUS_FORMAT(e, message)); } while(0)
#define LOG_EMERGENCY_STATUS(e, message) 			do { log_event(e->id, EMERGENCY_STATUS, EMERGENCY_STATUS_FORMAT(e, message)); } while(0)
#define LOG_EMERGENCY_STATUS_SHORT(e, message) 		do { log_event(e->id, EMERGENCY_STATUS, EMERGENCY_STATUS_SHORT_FORMAT(e, message)); } while(0)


int thread_updater(void *arg);
void update_rescuers_states_logging_blocking();
bool update_rescuer_digital_twin_state_logging_blocked(rescuer_digital_twin_t *t, int minx, int miny, int height, int width);
void send_rescuer_digital_twin_back_to_base_logging_blocked(rescuer_digital_twin_t *t);
void update_active_emergencies_states_logging_blocking();
void update_active_emergency_state_logging_blocked(emergency_t *e);
void update_waiting_emergencies_states();
void update_waiting_emergency_state(void *arg);
bool emergency_has_expired(void *arg);
void remove_expired_waiting_emergencies_logging();
bool emergency_is_to_promote(void *arg);
void promote_needing_waiting_emergencies();

#endif
