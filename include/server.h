#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include <sys/wait.h>
#include "log.h"
#include "parsers.h"
#include "server_utils.h"
#include "server_clock.h"
#include "server_updater.h"
#include "server_emergency_handler.h"
#include "emergency_requests_reciever.h"

#define THREAD_POOL_SIZE 10

#define LOG_IGNORE_EMERGENCY_REQUEST(m) log_event(NO_ID, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, m)
#define LOG_EMERGENCY_REQUEST_RECIVED() log_event(NO_ID, EMERGENCY_REQUEST_RECEIVED, "emergenza ricevuta e messa in attesa di essere processata!")

// server

typedef struct {
	environment_t *enviroment;
	rescuers_t *rescuers;		// struttura che contiene i tipi di rescuer e altri interi utili
	emergencies_t *emergencies;	// struttura che contiene i tipi di emergenza
	int emergency_requests_count;
	int valid_emergency_request_count;
	mtx_t rescuers_mutex;						// mutex per proteggere l'accesso ai rescuer types
	emergency_queue_t* waiting_queue;			// coda per contenere le emergenze da processare
	emergency_queue_t* working_queue;			// coda per contenere le emergenze assegnate a un thread
	emergency_list_t* completed_emergencies;	// lista per tenere traccia di tutte le emergenze completate
	emergency_list_t* canceled_emergencies; 	// lista delle emergenze cancellate
	mqd_t mq;									// message queue per ricevere le emergenze dai client	
	int tick;									// tick del server, per sincronizzare i thread
	int tick_count_since_start;					// contatore dei tick del server, per tenere traccia di quanti tick sono stati fatti
	mtx_t clock_mutex;							// mutex per proteggere l'accesso al tick del server
	cnd_t clock_updated;						// condizione per comunicare al therad updater di fare l'update
	atomic_bool server_must_stop;				// flag che segnala ai thread di fermarsi
} server_context_t;


// server.c
void funzione_per_sigchild(int sig);
void server(void);
void close_server(server_context_t *ctx);
server_context_t *mallocate_server_context();
void cleanup_server_context(server_context_t *ctx);


#endif
