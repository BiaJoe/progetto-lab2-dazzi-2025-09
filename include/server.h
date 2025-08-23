#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include <sys/wait.h>
#include "config.h"
#include "priority_queue.h"
#include "log.h"
#include "parsers.h"

#define THREAD_POOL_SIZE 10

#define LOG_IGNORE_EMERGENCY_REQUEST(m) log_event(NO_ID, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, m)
#define LOG_EMERGENCY_REQUEST_RECIVED() log_event(NO_ID, EMERGENCY_REQUEST_RECEIVED, "emergenza ricevuta e messa in attesa di essere processata!")

// STRUTTURE PER LE RICHIESTE DI EMERGENZA

typedef struct {
	char emergency_name[MAX_EMERGENCY_NAME_LENGTH];
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

// server

typedef struct {
	int count;
	int valid_count;
} requests_t;

typedef struct {
	bool tick;						// tick del server, per sincronizzare i thread
	int tick_count_since_start;		// contatore dei tick del server, per tenere traccia di quanti tick sono stati fatti
	mtx_t mutex;					// mutex per proteggere l'accesso al tick del server
	cnd_t updated;					// condizione per comunicare al therad updater di fare l'update
} clock_t;

typedef struct {
	environment_t *enviroment;
	rescuers_t *rescuers;		    // struttura che contiene i tipi di rescuer e altri interi utili
	emergencies_t *emergencies;	    // struttura che contiene i tipi di emergenza
	requests_t *requests;           // struttura che contiene il numero di richieste di emergenza totali e valide
	// mtx_t rescuers_mutex;		// mutex per proteggere l'accesso ai rescuer types
	pq_t* emergency_queue;			// coda per contenere le emergenze da processare
	pq_t* completed_emergencies;	// lista per tenere traccia di tutte le emergenze completate
	pq_t* canceled_emergencies; 	// lista delle emergenze cancellate
	clock_t *clock;					// struttura per gestire il clock del server
	atomic_bool server_must_stop;	// flag che segnala ai thread di fermarsi
	mqd_t mq;						// message queue per ricevere le emergenze dai client	
    int shm_fd;                	 	// file descriptior della SHM
    client_server_shm_t *shm;    	
    sem_t *sem_ready;            	
} server_context_t;


// server.c

void server_ipc_setup(server_context_t *ctx);
void close_server(server_context_t *ctx);
server_context_t *mallocate_server_context();
void cleanup_server_context(server_context_t *ctx);

// thread_clock.c

#define SERVER_TICK_INTERVAL_SECONDS 1
#define SERVER_TICK_INTERVAL_NANOSECONDS 0

int thread_clock(void *arg);
void tick(server_context_t *ctx);
void untick(server_context_t *ctx);
void wait_for_a_tick(server_context_t *ctx);
int server_is_ticking(server_context_t *ctx);
void lock_server_clock(server_context_t *ctx);
void unlock_server_clock(server_context_t *ctx);

// thread_reciever.c

int thread_reciever(server_context_t *ctx);
int parse_emergency_request(char *message, char* name, int *x, int *y, time_t *timestamp);
bool emergency_request_values_are_illegal(server_context_t *ctx, char* name, int x, int y, time_t timestamp);
emergency_t *mallocate_emergency(server_context_t *ctx, char* name, int x, int y, time_t timestamp);
void free_emergency(emergency_t* e);

// thread_worker.c

#define RESCUER_SEARCHING_FAIR_MODE 'f'
#define RESCUER_SEARCHING_STEAL_MODE 's'
#define TIME_INTERVAL_BETWEEN_RESCUERS_SEARCH_ATTEMPTS_SECONDS 3

int thread_worker(void *arg);

// thread_updater.c

int thread_updater(void *arg);
void update_rescuers_states_and_positions_on_the_map_logging(server_context_t *ctx);
bool update_rescuer_digital_twin_state_and_position_logging(rescuer_digital_twin_t *t, int minx, int miny, int height, int width);
void send_rescuer_digital_twin_back_to_base_logging(rescuer_digital_twin_t *t);
void change_rescuer_digital_twin_destination(rescuer_digital_twin_t *t, int new_x, int new_y);

// UTILS


//funzioni per lockare e unlockare mutex 




#endif
