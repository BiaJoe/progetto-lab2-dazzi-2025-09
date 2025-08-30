#include "server.h"
#include "config_server.h"

static int priority_count = (int)(sizeof(priority_lookup_table)/sizeof(priority_lookup_table[0]));
server_context_t *ctx = NULL;

int main(void){
	log_init(LOG_ROLE_SERVER, server_logging_config);
	init_server_context();		

	log_event(NON_APPLICABLE_LOG_ID, PARSING_STARTED, "Inizio parsing dei file di configurazione");
	ctx -> enviroment  = parse_env(ENV_CONF);														// ottengo l'ambiente
	ctx -> rescuers    = parse_rescuers(RESCUERS_CONF, ctx->enviroment->width, ctx->enviroment->height);	// ottengo i rescuers (mi servono max X e max Y)
	ctx -> emergencies = parse_emergencies(EMERGENCY_TYPES_CONF, ctx->rescuers, priority_lookup_table, priority_count);									// ottengo le emergenze (mi servono i rescuers per le richieste di rescuer)
	if(ctx->enviroment == NULL || ctx->rescuers == NULL || ctx->emergencies == NULL)
		log_error_and_exit(close_server, "Il parsing dei config file è fallito!");
	
	log_event(NON_APPLICABLE_LOG_ID, PARSING_ENDED, "Il parsing è terminato con successo!");
	
	server_ipc_setup(ctx); 
	log_event(NON_APPLICABLE_LOG_ID, SERVER, "IPC setup completato: coda MQ, SHM e semaforo pronti");
	log_event(NON_APPLICABLE_LOG_ID, SERVER, "tutte le variabili sono state ottenute dal server: adesso il sistema è a regime!");

	thrd_t clock_thread, updater_thread, receiver_thread, worker_threads[WORKER_THREADS_NUMBER];

    if (thrd_create(&clock_thread, thread_clock, NULL) != thrd_success)
        log_error_and_exit(close_server, "errore creazione clock");
    if (thrd_create(&updater_thread, thread_updater, NULL) != thrd_success)
        log_error_and_exit(close_server, "errore creazione updater");
    if (thrd_create(&receiver_thread, thread_reciever, NULL) != thrd_success)
        log_error_and_exit(close_server, "errore creazione receiver");
	for(int i = 0; i < WORKER_THREADS_NUMBER; i++) {
		if (thrd_create(&worker_threads[i], thread_worker, NULL) != thrd_success)
        	log_error_and_exit(close_server, "errore creazione receiver");
	}
	thrd_join(clock_thread, NULL);
	thrd_join(updater_thread, NULL);
	thrd_join(receiver_thread, NULL);
	
	for (int i = 0; i < WORKER_THREADS_NUMBER; i++) {
		thrd_join(worker_threads[i], NULL);
	}

	close_server(EXIT_SUCCESS);
	return 0;
}

void init_server_context() {
	ctx = (server_context_t *)malloc(sizeof(server_context_t));	
	check_error_memory_allocation(ctx);
	ctx -> requests = (requests_t *)malloc(sizeof(requests_t));
	check_error_memory_allocation(ctx -> requests);
	ctx -> clock = (server_clock_t *)malloc(sizeof(server_clock_t));
	check_error_memory_allocation(ctx -> clock);
	ctx -> active_emergencies = (active_emergencies_array_t *)calloc(1, sizeof(active_emergencies_array_t));
	check_error_memory_allocation(ctx -> clock);
	

	// popolo ctx
	ctx -> requests -> count = 0; 		// all'inizio non ci sono state ancora richieste
	ctx -> requests -> valid_count = 0;

	ctx -> clock -> tick_time = server_tick_time;	// prendo il tempo di un tick da config.h
	ctx -> clock -> tick = false;											
	ctx -> clock -> tick_count_since_start = 0; 			// il server non ha ancora fatto nessun tick
	check_error_cnd_init(cnd_init(&(ctx->clock->updated)));
	check_error_mtx_init(mtx_init(&(ctx->clock->mutex), mtx_plain));
	check_error_mtx_init(mtx_init(&(ctx->active_emergencies->mutex), mtx_plain));
	check_error_mtx_init(mtx_init(&(ctx->rescuers->mutex), mtx_plain));
	
	ctx -> emergency_queue = pq_create(priority_count);
	ctx -> completed_emergencies = pq_create(priority_count);
	ctx -> canceled_emergencies = pq_create(priority_count);
	ctx -> server_must_stop = false;
    
	ctx->mq      = (mqd_t)-1;
    ctx->shm_fd  = -1;
    ctx->shm     = NULL;
    ctx->sem_ready = SEM_FAILED;

	for (int i = 0; i < WORKER_THREADS_NUMBER; i++){
		ctx->active_emergencies->array[i] = NULL;
	}

	atomic_store(&ctx->active_emergencies->next_tw_index, 0);
	atomic_store(&ctx->server_must_stop, false);
}

void server_ipc_setup(){
    // niente residui
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH,
        .mq_curmsgs = 0
    };

    const char *qname = ctx->enviroment->queue_name;
    ERR_CHECK(qname == NULL || qname[0] != '/', "queue_name deve iniziare con '/'");

    mq_unlink(qname); // inoffensivo se non esiste
	SYSV(ctx->mq = mq_open(qname, O_CREAT | O_RDWR, 0644, &attr), MQ_FAILED, "mq_open");
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_SERVER, "coda emergenze pronta: %s", qname);

    // 2) SHM con il nome della coda
    SYSC(ctx->shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600), "shm_open");
    SYSC(ftruncate(ctx->shm_fd, (off_t)sizeof(client_server_shm_t)), "ftruncate");
    SYSV(ctx->shm = mmap(NULL, sizeof(*ctx->shm), PROT_READ | PROT_WRITE, MAP_SHARED, ctx->shm_fd, 0), MAP_FAILED, "mmap");

    memset(ctx->shm->queue_name, 0, sizeof(ctx->shm->queue_name));
    snprintf(ctx->shm->queue_name, sizeof(ctx->shm->queue_name), "%s", qname);
	ctx->shm->requests_argument_separator = EMERGENCY_REQUESTS_ARGUMENT_SEPARATOR_CHAR;
    SYSV(ctx->sem_ready = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 0), SEM_FAILED, "sem_open");
	SYSC(sem_post(ctx->sem_ready), "sem_post");
}

// libera tutto ciò che è contenuto nel server context e il server context stesso
void cleanup_server_context(){
	// segnalo ai threads di interrompersi
	atomic_store(&ctx->server_must_stop, true);
	// se il thread reciever fosse bloccato su mq_recieve, gli mando anche un messaggio di stop
	char *buffer = MQ_STOP_MESSAGE;
	SYSC(mq_send(ctx->mq, buffer, strlen(buffer) + 1, 0), "mq_send");

	// inizio lo smontaggio

	if (ctx->shm && ctx->shm != MAP_FAILED) SYSC(munmap(ctx->shm, sizeof(*ctx->shm)), "munmap");
    ctx->shm = NULL;
    if (ctx->shm_fd != -1) SYSC(close(ctx->shm_fd), "close");
    ctx->shm_fd = -1;
    if (ctx->sem_ready && ctx->sem_ready != SEM_FAILED) SYSC(sem_close(ctx->sem_ready), "sem_close");
    ctx->sem_ready = SEM_FAILED;
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

	free_rescuers(ctx->rescuers);
	ctx->rescuers = NULL;

	free_emergencies(ctx->emergencies);
	ctx->emergencies = NULL;

	pq_destroy(ctx->emergency_queue);
	pq_destroy(ctx->completed_emergencies);
	pq_destroy(ctx->canceled_emergencies);

	mq_close(ctx->mq);
	mq_unlink(ctx->enviroment->queue_name);

	free(ctx->enviroment);
	mtx_destroy(&(ctx->clock->mutex));
	cnd_destroy(&(ctx->clock->updated));
	mtx_destroy(&(ctx->active_emergencies->mutex));
	mtx_destroy(&(ctx->rescuers->mutex));
	free(ctx->clock);
	free(ctx->requests);
	free(ctx->active_emergencies);

	free(ctx);
}

// funzione di uscita, deve fare log_close()
void close_server(int exit_code){
	switch (exit_code) {
		case EXIT_SUCCESS:
			log_event(AUTOMATIC_LOG_ID, SERVER, "🏁  dopo %d updates, il serve si chiude  🏁", ctx->clock->tick_count_since_start);
			break;
		case EXIT_FAILURE:
			log_event(AUTOMATIC_LOG_ID, SERVER, "💀  dopo %d updates, qualcosa ha chiuso il server  💀", ctx->clock->tick_count_since_start);
			break;
		default:
			log_event(AUTOMATIC_LOG_ID, SERVER, "💀  dopo %d updates, qualcosa ha chiuso il server con messaggio sconosciuto 💀", ctx->clock->tick_count_since_start);
			exit_code = EXIT_FAILURE;
			break;
	}
	log_event(AUTOMATIC_LOG_ID, SERVER, "📋  Emergenze elaborate:  %d", ctx->requests->valid_count);
	log_event(AUTOMATIC_LOG_ID, SERVER, "✅  Emergenze completate: %d", ctx->completed_emergencies->size);
	log_close();
	cleanup_server_context();
	ctx = NULL;
	exit(exit_code);
}


int get_time_before_emergency_timeout_from_priority(int p){
	for (int i = 0; i < priority_count; i++){
		if(priority_lookup_table[i].number == p)
			return priority_lookup_table[i].time_before_timeout;
	}
	return INVALID_TIME;
}

// cerca tra le priorità e ritorna il loro livello nella tabella
// se non le trova ritorna la priorità minima 
// è una funzione interna, 
// lascia la responsabilità di fare il checking delle priorità a quelle di parsing
int get_priority_level(short priority_number, const priority_rule_t *table, int how_many_levels){
    if (!table || how_many_levels == 0) 
		return how_many_levels;
    bool found = false;
    for (int i = 0; i < how_many_levels; ++i) {
        if (table[i].number == priority_number) {
            found = true;
            break;
        }
    }
    if (!found) 
		return 0; 
	int level = 0;
    for (int i = 0; i < how_many_levels; ++i)
        if (table[i].number < priority_number) level++;

    return level;
}

int priority_to_level(short priority){
	return get_priority_level(priority, priority_lookup_table, priority_count);
}

short level_to_priority(int level){
	int index = 0;
	if (level < 0) index = 0;
	else if (level >= priority_count) index = priority_count-1;
	else index = level;
	return (short) priority_lookup_table[index].number;
}

int priority_to_timeout_timer(short priority) {
	int index = priority_to_level(priority);
	return (int) priority_lookup_table[index].time_before_timeout;
}

int priority_to_promotion_timer(short priority) {
	int index = priority_to_level(priority);
	return (int) priority_lookup_table[index].time_before_promotion;
}

short get_next_priority(short p) {
	int level = priority_to_level(p);
	if (level == priority_count-1) return p;
	return level_to_priority(level+1);
}

// ritorna > 0 se la prima è più grande
// ritorna < 0 se la seconda è più grande
// ritorna = 0 se sono uguali
int compare_priorities(short a, short b) {
	int aL = priority_to_level(a);
	int bL = priority_to_level(b);
	return aL - bL;
}
