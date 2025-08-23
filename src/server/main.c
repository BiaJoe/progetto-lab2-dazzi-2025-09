#include "server.h"

// struttura globale per contenere le strutture utili del server
server_context_t *ctx;
// prendo da config.h il numero di priorità ammesse
extern const int priority_count;

// divido in due processi: logger e server
int main(void){
	log_init(LOG_ROLE_SERVER);

	log_event(NON_APPLICABLE_LOG_ID, LOGGING_STARTED, "Inizio logging");	
	ctx = mallocate_server_context();		
	log_event(NON_APPLICABLE_LOG_ID, PARSING_STARTED, "Inizio parsing dei file di configurazione");
	ctx -> enviroment  = parse_env();														// ottengo l'ambiente
	ctx -> rescuers    = parse_rescuers(ctx->enviroment->width, ctx->enviroment->height);	// ottengo i rescuers (mi servono max X e max Y)
	ctx -> emergencies = parse_emergencies(ctx->rescuers);									// ottengo le emergenze (mi servono i rescuers per le richieste di rescuer)
	log_event(NON_APPLICABLE_LOG_ID, PARSING_ENDED, "Il parsing è terminato con successo!");
	server_ipc_setup(ctx); // setup IPC: MQ, SHM, semaforo
	log_event(NON_APPLICABLE_LOG_ID, SERVER, "IPC setup completato: coda MQ, SHM e semaforo pronti");
	log_event(NON_APPLICABLE_LOG_ID, SERVER, "tutte le variabili sono state ottenute dal server: adesso il sistema è a regime!");

	thrd_t clock_thread;
	thrd_t updater_thread;
	thrd_t reciever_thread;
	thrd_t worker_threads[THREAD_POOL_SIZE];

	if (thrd_create(&clock_thread, thread_clock, ctx) != thrd_success) {
		log_fatal_error("errore durante la creazione di del clock nel server");
		return;
	}

	if (thrd_create(&updater_thread, thread_updater, ctx) != thrd_success) {
		log_fatal_error("errore durante la creazione di dell'updater nel server");
		return;
	}

	if (thrd_create(&updater_thread, thread_reciever, ctx) != thrd_success) {
		log_fatal_error("errore durante la creazione di dell'reciever nel server");
		return;
	}

	thrd_join(clock_thread, NULL);
	thrd_join(updater_thread, NULL);
	thrd_join(reciever_thread, NULL);

	// for (int i = 0; i < THREAD_POOL_SIZE; i++) {
	// 	thrd_join(worker_threads[i], NULL);
	// }

	close_server(ctx);
}

server_context_t *mallocate_server_context(){
	server_context_t *ctx = (server_context_t *)malloc(sizeof(server_context_t));	
	check_error_memory_allocation(ctx);
	ctx -> requests = (requests_t *)malloc(sizeof(requests_t));
	check_error_memory_allocation(ctx -> requests);
	ctx -> clock = (clock_t *)malloc(sizeof(clock_t));
	check_error_memory_allocation(ctx -> clock);

	// popolo ctx
	ctx -> requests -> count = 0; 		// all'inizio non ci sono state ancora richieste
	ctx -> requests -> valid_count = 0;

	ctx -> clock -> tick = false;											
	ctx -> clock -> tick_count_since_start = 0; 			// il server non ha ancora fatto nessun tick
	check_error_cnd_init(cnd_init(&(ctx->clock->updated)));
	check_error_mtx_init(mtx_init(&(ctx->clock->mutex), mtx_plain));
	
	ctx -> emergency_queue = pq_create(priority_count);
	ctx -> completed_emergencies = pq_create(priority_count);
	ctx -> canceled_emergencies = pq_create(priority_count);
	ctx -> server_must_stop = false;
    
	ctx->mq      = (mqd_t)-1;
    ctx->shm_fd  = -1;
    ctx->shm     = NULL;
    ctx->sem_ready = SEM_FAILED;

	// check_error_mtx_init(mtx_init(&(ctx->rescuers_mutex), mtx_plain));

    return ctx;
}

void server_ipc_setup(server_context_t *ctx){
    // sicurezza: niente residui
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 64,
        .mq_msgsize = MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH,
        .mq_curmsgs = 0
    };

    const char *qname = ctx->enviroment->queue_name;
    ERR_CHECK(qname == NULL || qname[0] != '/', "queue_name deve iniziare con '/'");

    mq_unlink(qname); // inoffensivo se non esiste
	SYSV(ctx->mq = mq_open(qname, O_CREAT | O_RDONLY, 0644, &attr), MQ_FAILED, "mq_open");
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_SERVER, "coda emergenze pronta: %s", qname);

    // 2) SHM con il nome della coda
    SYSC(ctx->shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600), "shm_open");
    SYSC(ftruncate(ctx->shm_fd, (off_t)sizeof(client_server_shm_t)), "ftruncate");
    SYSV(ctx->shm = mmap(NULL, sizeof(*ctx->shm), PROT_READ | PROT_WRITE, MAP_SHARED, ctx->shm_fd, 0), MAP_FAILED, "mmap");

    memset(ctx->shm->queue_name, 0, sizeof(ctx->shm->queue_name));
    strncpy(ctx->shm->queue_name, qname, sizeof(ctx->shm->queue_name) - 1);

    // 3) Semaforo "ready" (latch). Server fa N post per n lanci client.
    SYSV(ctx->sem_ready = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 0), SEM_FAILED, "sem_open");
}

// libera tutto ciò che è contenuto nel server context e il server context stesso
void cleanup_server_context(server_context_t *ctx){
	if (ctx->shm && ctx->shm != MAP_FAILED) SYSC(munmap(ctx->shm, sizeof(*ctx->shm)), "munmap");
    ctx->shm = NULL;
    if (ctx->shm_fd != -1) SYSC(close(ctx->shm_fd), "close");
    ctx->shm_fd = -1;
    if (ctx->sem_ready && ctx->sem_ready != SEM_FAILED) SYSC(sem_close(ctx->sem_ready), "sem_close");
    ctx->sem_ready = SEM_FAILED;
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

	free_rescuer_types(ctx->rescuers->types);
	free(ctx->rescuers);
	free_emergency_types(ctx->emergencies->types);
	free(ctx->emergencies);

	pq_destroy(ctx->emergency_queue);
	pq_destroy(ctx->completed_emergencies);
	pq_destroy(ctx->canceled_emergencies);

	mq_close(ctx->mq);
	mq_unlink(ctx->enviroment->queue_name);
	free(ctx->enviroment->queue_name);
	free(ctx->enviroment);
	mtx_destroy(&(ctx->clock->mutex));
	// mtx_destroy(&(ctx->rescuers_mutex));
	cnd_destroy(&(ctx->clock->updated));

	free(ctx);
}


void close_server(server_context_t *ctx){
	log_event(AUTOMATIC_LOG_ID, SERVER, "dopo %d updates, il serve si chiud. Sono state elaborate %d emergenze, di cui %d completate", ctx->clock->tick_count_since_start, ctx->requests->valid_count, ctx->completed_emergencies->size);
	log_event(AUTOMATIC_LOG_ID, LOGGING_ENDED, "Fine del logging");
	cleanup_server_context(ctx);
	exit(EXIT_SUCCESS);
}


