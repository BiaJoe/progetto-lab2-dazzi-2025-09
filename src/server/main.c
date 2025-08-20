#include "server.h"

void funzione_per_sigchild(int sig) {
	(void)sig; // per zittire il compilatore (unused parameter)
	wait(NULL); 
}

// divido in due processi: logger e server
// mentre il logger aspetta per messaggi da loggare
// il server fa il parsing e aspetta per richieste di emergenza
int main(void){
	log_init(LOG_ROLE_SERVER);
	return 0;
}

void server(void){
	log_event(NON_APPLICABLE_LOG_ID, LOGGING_STARTED, "Inizio logging");		// si inizia a loggare
	log_event(NON_APPLICABLE_LOG_ID, PARSING_STARTED, "Inizio parsing dei file di configurazione");
	server_context_t *ctx = mallocate_server_context();											// estraggo le informazioni dai file conf, le metto tutte nel server context
	log_event(NON_APPLICABLE_LOG_ID, PARSING_ENDED, "Il parsing è terminato con successo!");
	log_event(NON_APPLICABLE_LOG_ID, SERVER, "tutte le variabili sono state ottenute dal server: adesso il sistema è a regime!");

	thrd_t clock_thread;
	thrd_t updater_thread;
	thrd_t worker_threads[THREAD_POOL_SIZE];

	if (thrd_create(&clock_thread, server_clock, ctx) != thrd_success) {
		log_fatal_error("errore durante la creazione di del clock nel server");
		return;
	}

	if (thrd_create(&updater_thread, server_updater, ctx) != thrd_success) {
		log_fatal_error("errore durante la creazione di dell'updater nel server");
		return;
	}
	
	for (int i = 0; i < THREAD_POOL_SIZE; i++) {
		if (thrd_create(&worker_threads[i], server_emergency_handler, ctx) != thrd_success) {
			log_fatal_error("errore durante la creazione di un elaboratore di emergenze nel server");
			return;
		}
	}

	emergency_requests_reciever(ctx);

	// arrivati qui il server deve chiudersi faccio il join dei thread così quando hanno finito chiudo tutto
	thrd_join(clock_thread, NULL);
	thrd_join(updater_thread, NULL);
	for (int i = 0; i < THREAD_POOL_SIZE; i++) {
		thrd_join(worker_threads[i], NULL);
	}

	close_server(ctx);
}


// faccio il parsing dei file
// ottengo numero e puntatore ad emergenze e rescuers
// creo la coda di messaggi ricevuti dal client
// inizializzo i mutex
server_context_t *mallocate_server_context(){
	server_context_t *ctx = (server_context_t *)malloc(sizeof(server_context_t));	
	check_error_memory_allocation(ctx);

	// Parsing dei file di configurazione
	ctx -> enviroment  = parse_env();														// ottengo l'ambiente
	ctx -> rescuers    = parse_rescuers(ctx->enviroment->width, ctx->enviroment->height);	// ottengo i rescuers (mi servono max X e max Y)
	ctx -> emergencies = parse_emergencies(ctx->rescuers);									// ottengo le emergenze (mi servono i rescuers per le richieste di rescuer)
	
	// popolo ctx
	ctx -> emergency_requests_count = 0; 		// all'inizio non ci sono state ancora richieste
	ctx -> valid_emergency_request_count = 0;
	ctx -> tick = NO;											
	ctx -> tick_count_since_start = 0; 			// il server non ha ancora fatto nessun tick
	ctx -> waiting_queue = mallocate_emergency_queue();
	ctx -> working_queue = mallocate_emergency_queue();
	ctx -> completed_emergencies = mallocate_emergency_list(); 
	ctx -> canceled_emergencies = mallocate_emergency_list();
	ctx -> server_must_stop = false;

	struct mq_attr attr = {
		.mq_flags = 0,
		.mq_maxmsg = 10,
		.mq_msgsize = MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH,
		.mq_curmsgs = 0
	};

	char queue_name[MAX_EMERGENCY_QUEUE_NAME_LENGTH + 1];
	snprintf(queue_name, MAX_EMERGENCY_QUEUE_NAME_LENGTH, "/%s", ctx->enviroment->queue_name);
	mq_unlink(queue_name);
	ctx->mq = mq_open(queue_name, O_CREAT | O_RDONLY, 0644, &attr);
	check_error_mq_open(ctx->mq);

	check_error_mtx_init(mtx_init(&(ctx->clock_mutex), mtx_plain));
	check_error_mtx_init(mtx_init(&(ctx->rescuers_mutex), mtx_plain));
	check_error_cnd_init(cnd_init(&(ctx->clock_updated)));

	return ctx; 																									// ritorno il contesto del server
}

// libera tutto ciò che è contenuto nel server context e il server context stesso
void cleanup_server_context(server_context_t *ctx){
	free_rescuer_types(ctx->rescuer_types);
	free_emergency_types(ctx->emergency_types);
	free_emergency_queue(ctx->waiting_queue);
	free_emergency_queue(ctx->working_queue);
	free_emergency_list(ctx->completed_emergencies);
	free_emergency_list(ctx->canceled_emergencies);

	mq_close(ctx->mq);
	mq_unlink(EMERGENCY_QUEUE_NAME);
	mtx_destroy(&(ctx->clock_mutex));
	mtx_destroy(&(ctx->rescuers_mutex));
	cnd_destroy(&(ctx->clock_updated));

	free(ctx);
}


void close_server(server_context_t *ctx){
	log_event(AUTOMATIC_LOG_ID, SERVER, "dopo %d updates, il serve si chiud. Sono state elaborate %d emergenze, di cui %d completate", ctx->tick_count_since_start, ctx->valid_emergency_request_count, ctx->completed_emergencies->node_amount);
	log_event(AUTOMATIC_LOG_ID, LOGGING_ENDED, "Fine del logging");
	cleanup_server_context(ctx);
	exit(EXIT_SUCCESS);
}


