#include "server.h"
#include "config_server.h"

static int priority_count = (int)(sizeof(priority_lookup_table)/sizeof(priority_lookup_table[0]));
server_context_t *ctx;
static int timeout_count = 0, canceled_count = 0, waiting_count = 0;
static atomic_int  exit_code = ATOMIC_VAR_INIT(EXIT_SUCCESS);
static atomic_bool exited    = ATOMIC_VAR_INIT(false);



int main(void){
	log_init(LOG_ROLE_SERVER, server_logging_config);

	sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);   
    sigaddset(&sigset, SIGTERM);  
    sigprocmask(SIG_BLOCK, &sigset, NULL);

	init_server_context();		

	log_event(NON_APPLICABLE_LOG_ID, PARSING_STARTED, "Inizio parsing dei file di configurazione");
	ctx -> enviroment  = parse_env(ENV_CONF);														
	ctx -> rescuers    = parse_rescuers(RESCUERS_CONF, ctx->enviroment->width, ctx->enviroment->height);	
	ctx -> emergencies = parse_emergencies(EMERGENCY_TYPES_CONF, ctx->rescuers, priority_lookup_table, priority_count);	
	
	if(ctx->enviroment == NULL || ctx->rescuers == NULL || ctx->emergencies == NULL)
		log_error_and_exit(close_server, "Il parsing dei config file è fallito!");
	
	log_event(NON_APPLICABLE_LOG_ID, PARSING_ENDED, "Il parsing è terminato con successo!");
	
	json_visualizer_begin(SIMULATION_JSON, ctx->enviroment->width, ctx->enviroment->height);

	server_ipc_setup(); 

	log_event(NON_APPLICABLE_LOG_ID, SERVER, "IPC setup completato: coda MQ, SHM e semaforo pronti");
	log_event(NON_APPLICABLE_LOG_ID, SERVER, "tutte le variabili sono state ottenute dal server: adesso il sistema è a regime!");

	int main_threads_started, workers_started;
	thrd_t clock_thread, updater_thread, receiver_thread, worker_threads[WORKER_THREADS_NUMBER];

	// faccio partire i thread
	bool all_thread_started = start_threads(
		&clock_thread, 
		&updater_thread, 
		&receiver_thread, 
		worker_threads, 
		&main_threads_started, 
		&workers_started
	);

	// se qualcosa fallisce chiudo
	if (!all_thread_started) { 
		if (main_threads_started >= 3) thrd_join(receiver_thread, NULL);
		if (main_threads_started >= 1) thrd_join(clock_thread, NULL);
		if (main_threads_started >= 2) thrd_join(updater_thread, NULL);

		for (int i = 0; i < workers_started; ++i)
			thrd_join(worker_threads[i], NULL);

		close_server(EXIT_FAILURE); 
	}

	// il sistema è a regime: aspetto che qualcosa lo fermi
	int sig = 0;
    sigwait(&sigset, &sig);  
    log_event(AUTOMATIC_LOG_ID, SERVER, "🛑 Segnale %d: spegnimento ordinato", sig);

    atomic_store(&ctx->server_must_stop, true);
	mq_send(ctx->mq, MQ_STOP_MESSAGE, strlen(MQ_STOP_MESSAGE) + 1, 0);
    if (ctx && ctx->emergency_queue) pq_close(ctx->emergency_queue); 

    // sveglio clock + workers
    cnd_broadcast(&ctx->clock->updated);
    lock_emergencies();
    for (int i=0; i<WORKER_THREADS_NUMBER; ++i) {
        emergency_t *e = ctx->active_emergencies->array[i];
        if (e) cnd_broadcast(&e->cond);
    }
    unlock_emergencies();
	
	thrd_join(receiver_thread, NULL);
	thrd_join(clock_thread, NULL);
	thrd_join(updater_thread, NULL);
	
	for (int i = 0; i < WORKER_THREADS_NUMBER; i++) {
		thrd_join(worker_threads[i], NULL);
	}

	close_server(atomic_load(&exit_code));
	return 0;
}


bool start_threads(
    thrd_t *clock_thread,
    thrd_t *updater_thread,
    thrd_t *receiver_thread,
    thrd_t worker_threads[WORKER_THREADS_NUMBER],
    int *main_threads_started,
    int *workers_started
) {
    *main_threads_started = 0;
    *workers_started = 0;
    // clock
    if (thrd_create(clock_thread, thread_clock, NULL) != thrd_success) {
        log_error_and_exit(request_shutdown_from_thread, "errore creazione clock");
        return false;
    }
    (*main_threads_started)++;
    // updater
    if (thrd_create(updater_thread, thread_updater, NULL) != thrd_success) {
        log_error_and_exit(request_shutdown_from_thread, "errore creazione updater");
        return false;
    }
    (*main_threads_started)++;
    // receiver
    if (thrd_create(receiver_thread, thread_reciever, NULL) != thrd_success) {
        log_error_and_exit(request_shutdown_from_thread, "errore creazione receiver");
        return false;
    }
    (*main_threads_started)++;
    // workers
    for (int i = 0; i < WORKER_THREADS_NUMBER; ++i) {
        if (thrd_create(&worker_threads[i], thread_worker, NULL) != thrd_success) {
            log_error_and_exit(request_shutdown_from_thread, "errore creazione worker #%d", i);
            return false;
        }
        (*workers_started)++;
    }
    return true;
}

void init_server_context() {	
	ctx = (server_context_t *)malloc(sizeof(server_context_t));	
	check_error_memory_allocation(ctx);
	ctx -> requests = (requests_t *)malloc(sizeof(requests_t));
	check_error_memory_allocation(ctx->requests);
	ctx -> clock = (server_clock_t *)malloc(sizeof(server_clock_t));
	check_error_memory_allocation(ctx->clock);
	ctx -> active_emergencies = (active_emergencies_array_t *)calloc(1, sizeof(active_emergencies_array_t));
	check_error_memory_allocation(ctx->active_emergencies);

	// popolo ctx
	ctx -> requests -> count = 0; 		// all'inizio non ci sono state ancora richieste
	ctx -> requests -> valid_count = 0;

	ctx -> clock -> tick_time = server_tick_time;	// prendo il tempo di un tick da config.h
	ctx -> clock -> tick = false;		
	
	atomic_store(&ctx->clock->tick_count_since_start, 0); 			// il server non ha ancora fatto nessun tick
	check_error_cnd_init(cnd_init(&(ctx->clock->updated)));
	check_error_mtx_init(mtx_init(&(ctx->clock->mutex), mtx_plain));
	check_error_mtx_init(mtx_init(&(ctx->active_emergencies->mutex), mtx_plain));

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
	snprintf(ctx->shm->config.env, FILENAME_BUFF, "%s", ENV_CONF);
	snprintf(ctx->shm->config.rescuer_types, FILENAME_BUFF, "%s", RESCUERS_CONF);
	snprintf(ctx->shm->config.emergency_types, FILENAME_BUFF, "%s", EMERGENCY_TYPES_CONF);
	ctx->shm->server_pid = getpid();
    SYSV(ctx->sem_ready = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 0), SEM_FAILED, "sem_open");
	SYSC(sem_post(ctx->sem_ready), "sem_post");
}

static void wake_everyone_now(void) {
    cnd_broadcast(&ctx->clock->updated);

    lock_emergencies();
    for (int i = 0; i < WORKER_THREADS_NUMBER; ++i) {
        emergency_t *e = ctx->active_emergencies->array[i];
        if (e) cnd_broadcast(&e->cond);
    }
    unlock_emergencies();
}

void request_shutdown_from_thread(int code) {
    int final = (code == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (final == EXIT_FAILURE) atomic_store(&exit_code, EXIT_FAILURE);
    bool already = atomic_exchange(&exited, true);
    if (already) return;

    atomic_store(&ctx->server_must_stop, true);
    // sblocco chi attende su pq
    if (ctx && ctx->emergency_queue) pq_close(ctx->emergency_queue);
    wake_everyone_now();
	// chiudo la message queue del reciever con un messaggio che lo fa uscire
    if (ctx && ctx->mq != (mqd_t)-1) {
        const char *pp = MQ_STOP_MESSAGE;
        (void)mq_send(ctx->mq, pp, strlen(pp) + 1, 0);
    }
}

// libera tutto ciò che è contenuto nel server context e il server context stesso
void cleanup_server_context(){
	// segnalo ai threads di interrompersi
	atomic_store(&ctx->server_must_stop, true);
	// inizio lo smontaggio
	if (ctx->shm && ctx->shm != MAP_FAILED) SYSC(munmap(ctx->shm, sizeof(*ctx->shm)), "munmap");
    ctx->shm = NULL;
    if (ctx->shm_fd != -1) SYSC(close(ctx->shm_fd), "close");
    ctx->shm_fd = -1;
    if (ctx->sem_ready && ctx->sem_ready != SEM_FAILED) SYSC(sem_close(ctx->sem_ready), "sem_close");
    ctx->sem_ready = SEM_FAILED;
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
	
	json_visualizer_end();

	pq_destroy(ctx->emergency_queue);
	pq_destroy(ctx->completed_emergencies);
	pq_destroy(ctx->canceled_emergencies);

	mq_close(ctx->mq);
	mq_unlink(ctx->enviroment->queue_name);

	mtx_destroy(&(ctx->clock->mutex));
	cnd_destroy(&(ctx->clock->updated));
	mtx_destroy(&(ctx->active_emergencies->mutex));

	free(ctx->enviroment); 
	ctx->enviroment = NULL;
	free_emergencies(ctx->emergencies); 
	ctx->emergencies = NULL;
	free_rescuers(ctx->rescuers); 
	ctx->rescuers = NULL;
	free(ctx->clock); 
	ctx->clock = NULL;
	free(ctx->requests); 
	ctx->requests = NULL;
	free(ctx->active_emergencies); 
	ctx->active_emergencies = NULL;

	free(ctx);
	ctx = NULL;
}

static void count_pq_timeout(void *item) {
    emergency_t *e = (emergency_t*)item;
    if (e && e->status == TIMEOUT)
        timeout_count++;
}

static void count_pq_canceled(void *item) {
    emergency_t *e = (emergency_t*)item;
    if (e && e->status == CANCELED)
        canceled_count++;
}

static void count_pq_waiting(void *item) {
    emergency_t *e = (emergency_t*)item;
    if (e) waiting_count++;
}


// funzione di uscita, deve fare log_close()
void close_server(int code){
	switch (code) {
		case EXIT_SUCCESS:
			log_event(AUTOMATIC_LOG_ID, SERVER, "🏁  dopo %d updates, il serve si chiude  🏁", ctx->clock->tick_count_since_start);
			break;
		case EXIT_FAILURE:
			log_event(AUTOMATIC_LOG_ID, SERVER, "💀  dopo %d updates, qualcosa ha chiuso il server  💀", ctx->clock->tick_count_since_start);
			break;
		default:
			log_event(AUTOMATIC_LOG_ID, SERVER, "💀  dopo %d updates, qualcosa ha chiuso il server con messaggio sconosciuto 💀", ctx->clock->tick_count_since_start);
			code = EXIT_FAILURE;
			break;
	}

	json_visualizer_positions_dump(POSITIONS_JSON, POSITIONS_TXT);

	timeout_count = 0; canceled_count = 0; waiting_count = 0;
	pq_map(ctx->canceled_emergencies, count_pq_timeout);
	pq_map(ctx->canceled_emergencies, count_pq_canceled);
	pq_map(ctx->emergency_queue, count_pq_waiting);
	log_event(AUTOMATIC_LOG_ID, SERVER, "📋  Emergenze elaborate:         %d", ctx->requests->valid_count);
	log_event(AUTOMATIC_LOG_ID, SERVER, "✅  Emergenze completate:        %d", ctx->completed_emergencies->size);
	log_event(AUTOMATIC_LOG_ID, SERVER, "❌  Emergenze andate in timeout:  %d", timeout_count);
	log_event(AUTOMATIC_LOG_ID, SERVER, "💀  Emergenze cancellate:        %d", canceled_count);
	log_event(AUTOMATIC_LOG_ID, SERVER, "🕰️  Emergenze rimaste in attesa: %d", waiting_count);


	log_close();

	atomic_store(&ctx->server_must_stop, true);

	lock_emergencies();
    for (int i = 0; i < WORKER_THREADS_NUMBER; ++i) {
        emergency_t *e = ctx->active_emergencies->array[i];
        if (e) cnd_broadcast(&e->cond);   // sveglia qualunque worker in attesa
    }
    unlock_emergencies();

	cleanup_server_context();
	ctx = NULL;
	exit(code);
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
