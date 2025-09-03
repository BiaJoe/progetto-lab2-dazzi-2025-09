#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "errors.h"
#include "config_log.h"
#include "log.h"

static log_role_t    log_role;
static mqd_t         log_mq_reader = (mqd_t)-1;   // server: riceve
static mqd_t         log_mq_writer = (mqd_t)-1;   // client/server/altro: scrive
static thrd_t        logger_thread_id;
static atomic_bool   logger_running = false;
static atomic_int    flushed_stdout_n_times = 0;
static bool          queue_was_opened = false; 
static mtx_t         stdout_flush_mutex;
static mtx_t         thread_names_mutex;
static thread_name_t thread_names[MAX_LOG_THREADS];



static logging_config_t config;
static atomic_int log_event_counters[LOG_EVENT_TYPES_COUNT];

const log_event_info_t* get_log_event_info(log_event_type_t e) {
    if (e < 0 || e >= LOG_EVENT_TYPES_COUNT) return &LOG_EVENT_LOOKUP_TABLE[NON_APPLICABLE];
    return &LOG_EVENT_LOOKUP_TABLE[e];
}

// funzione per scrivere una riga di log
static void logger_write_line(log_message_t m) {
    if (!config.log_to_file || !config.log_file_ptr)
        return;
    char text[LOG_EVENT_TOTAL_LENGTH];

    // se il messaggio non viene dal processo che scrive sul file
    // ovvero se il messaggio viene dal client
    // lo riassemblo per ricalcolare gli id automatici 
    // utilizzando i counter del server
    if(m.role != log_role) {
        assemble_log_text(text, m);
        fprintf(config.log_file_ptr, "%s", text);
        return;
    }
    // se sono qui invece sono nel server, il messaggio è gia pronto in m.text
    // dove lo ha messo log_event
    fprintf(config.log_file_ptr, "%s", m.text);
}

// thread che legge i messaggi dalla coda e li scrive nel file di log e/o in stdout
// usato solo dal server
static int logger_thread(void *arg) {
    (void)arg;

    if (config.log_to_file && !config.log_file_ptr) {
        config.log_file_ptr = fopen(config.log_file, "a");
        check_error_fopen(config.log_file_ptr);
    }

    int lines_logged_since_last_flush = 0;

    while (atomic_load(&logger_running)) {
		log_message_t m;
		ssize_t n = mq_receive(log_mq_reader, (char*)&m, sizeof(m), NULL);
		check_error_mq_receive(n);
        m.text[sizeof(m.text) - 1] = '\0';

		const log_event_info_t *info = get_log_event_info(m.event_type);
		if (!info || info->is_to_log == 0) { continue; }

		// scrivi la riga nel log
		logger_write_line(m);
		lines_logged_since_last_flush++;
		if (info->is_terminating && m.role == log_role) // solo se il server ha loggato un evento che fa terminare ci si ferma, non il client
            atomic_store(&logger_running, false);
		if (lines_logged_since_last_flush >= config.flush_every_n) {
            lines_logged_since_last_flush = 0;
            if(config.log_to_file && config.log_file_ptr) 
                fflush(config.log_file_ptr);
    	}
	}

    if(config.log_to_file && config.log_file_ptr) {
        fflush(config.log_file_ptr);
        fclose(config.log_file_ptr);
        config.log_file_ptr = NULL;
    }

    return 0;
}

// inizializza il sistema di logging
// se siamo nel server il ruolo è di ricevere e anche di scrivere messaggi
// e anche di creare il thread che li scriverà nel logfile o in stdout
// altrimenti solo di scrivere
int log_init(log_role_t role, logging_config_t logging_config){
    
    log_role = role;
    config = logging_config;
    flushed_stdout_n_times = 0;

    check_error_mtx_init(mtx_init(&stdout_flush_mutex, mtx_plain));
    check_error_mtx_init(mtx_init(&thread_names_mutex, mtx_plain));   // 👈 mutex per tabella

    // azzera tabella thread_name
    for (int i = 0; i < MAX_LOG_THREADS; i++) {
        thread_names[i].tid = 0;          // valore sentinella = slot vuoto
        thread_names[i].name[0] = '\0';
    }

    atomic_store(&logger_running, (role == LOG_ROLE_SERVER ? true : false));  // logger_running serve solo al thread logger dentro al server


    if (role == LOG_ROLE_SERVER) {
        if (config.log_to_file) {                 
            struct mq_attr attr = {0};
            attr.mq_flags   = 0;
            attr.mq_maxmsg  = MAX_LOG_QUEUE_MESSAGES;
            attr.mq_msgsize = sizeof(log_message_t);

            mq_unlink(LOG_QUEUE_NAME);
            log_mq_reader = mq_open(LOG_QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
            check_error_mq_open(log_mq_reader);

            log_mq_writer = mq_open(LOG_QUEUE_NAME, O_WRONLY);
            check_error_mq_open(log_mq_writer);

            queue_was_opened = true;

            check_error_thread_create(thrd_create(&logger_thread_id, logger_thread, NULL));
        } else  queue_was_opened = false;

        log_register_this_thread("SERVER");
        log_event(NON_APPLICABLE_LOG_ID, LOGGING_STARTED, "(SERVER) Inizio del Logging!");

    } else {
        // non siamo nel server
        log_mq_writer = mq_open(LOG_QUEUE_NAME, O_WRONLY);
        if (log_mq_writer == (mqd_t)-1) {
            for (int i = 0; i < LOG_QUEUE_OPEN_MAX_RETRIES && log_mq_writer == (mqd_t)-1; ++i) {
                struct timespec t = { .tv_sec = 0, .tv_nsec = LOG_QUEUE_OPEN_RETRY_INTERVAL_NS };
                thrd_sleep(&t, NULL);
                log_mq_writer = mq_open(LOG_QUEUE_NAME, O_WRONLY);
            }
        }
        if (!(log_mq_writer == (mqd_t)-1)){
            queue_was_opened = true;
        }
         log_register_this_thread("CLIENT");
        log_event(NON_APPLICABLE_LOG_ID, LOGGING_STARTED, "(CLIENT) Inizio del Logging!");

    }
	return 0;
}


// chiude il sistema di logging
// se siamo nel server chiude il thread che scrive i log e la coda di logging
// se siamo nel client chiude solo la coda di logging
// LOGGA la chiusura, serve per svegloare la mq_receive nel thread logger
void log_close(void) {
    // quest'ultuimo evento serve a loggare e anche a far chiudere il logger thread se siamo nel server
    if (log_role == LOG_ROLE_CLIENT) {
        log_event(NON_APPLICABLE_LOG_ID, LOGGING_ENDED, "(CLIENT) Logging terminato :)");
        if (log_mq_writer != (mqd_t)-1) mq_close(log_mq_writer);
        if (config.log_to_stdout) fflush(stdout);
        mtx_destroy(&stdout_flush_mutex);
        return;
    }

    // siamo nel server
    log_event(NON_APPLICABLE_LOG_ID, LOGGING_ENDED, "(SERVER) Logging terminato :)");

    
    if (config.log_to_file && queue_was_opened) {
        thrd_join(logger_thread_id, NULL);
        atomic_store(&logger_running, false);

        if (log_mq_writer != (mqd_t)-1) { mq_close(log_mq_writer); log_mq_writer = (mqd_t)-1; }
        if (log_mq_reader != (mqd_t)-1) { mq_close(log_mq_reader); mq_unlink(LOG_QUEUE_NAME); log_mq_reader = (mqd_t)-1; }
        queue_was_opened = false;
    }

    if (config.log_to_stdout) fflush(stdout);
    mtx_destroy(&stdout_flush_mutex);
}

void assemble_log_text(char destination_string[LOG_EVENT_TOTAL_LENGTH], log_message_t m){
    // estraggo la stringa del nome del tipo di evento
    const log_event_info_t *info = get_log_event_info(m.event_type);
    const char *event_name = info ? info->name : "NOME_NON_TROVATO";

    
    // preparo l'ID come stringa includendo i casi particolari
    int idx = (m.event_type >= 0 && m.event_type < LOG_EVENT_TYPES_COUNT) ? (int)m.event_type : NON_APPLICABLE;
    char out_id[MAX_LOG_EVENT_ID_STRING_LENGTH];
    switch (m.id) {
        case AUTOMATIC_LOG_ID:
            snprintf(out_id, sizeof(out_id), "%d", atomic_fetch_add(&log_event_counters[idx], 1));
            break;
        case NON_APPLICABLE_LOG_ID:
            snprintf(out_id, sizeof(out_id), "%s", config.non_applicable_log_id_string);
            break;
        default: 
            snprintf(out_id, sizeof(out_id), "%d", m.id);
            break;
    }
    snprintf(destination_string, LOG_EVENT_TOTAL_LENGTH, 
        config.logging_syntax, 
        m.timestamp, out_id, 
        event_name, 
        m.thread_name, 
        m.formatted_message
    );
}

// ho imparato come usare variable argument lists sul libro: The C Programming Language, 2nd Edition
void log_event(int id, log_event_type_t event_type, char *format, ...) {
    const log_event_info_t *info = get_log_event_info(event_type);
    if (!info || !info->is_to_log) return;
    log_message_t msg = { 0 };
    
    // formo il messaggio formattato 
    char message[MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH];
    va_list ap;
    va_start(ap, format);
    vsnprintf(message, MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH, format, ap);
    va_end(ap);
    message[sizeof(message) - 1] = '\0'; // mi assicuro che l'ultimo carattere sia il terminatore
   
    // segno il timestamp in cui ho loggato
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long seconds      = ts.tv_sec;
    long milliseconds = ts.tv_nsec / 1000000; // da ns a ms
    char timestamp[MAX_LOG_EVENT_TIMESTAMP_STRING_LENGTH];
    snprintf(timestamp, sizeof(timestamp), "%ld%03ld", seconds, milliseconds);

    // preparo la struttura del messaggio
    msg.role = log_role;
    snprintf(msg.timestamp, sizeof(msg.timestamp), "%s", timestamp);
    msg.id = id;
    msg.event_type = event_type;
    snprintf(msg.formatted_message, sizeof(msg.formatted_message), "%s", message);
    const char *tname = log_get_current_thread_name();
    snprintf(msg.thread_name, sizeof(msg.thread_name), "%s", tname);
    msg.thread_name[sizeof(msg.thread_name) - 1] = '\0';

    assemble_log_text(msg.text, msg);

    if (config.log_to_stdout){ // se è previsto, scrivo su stdout
        mtx_lock(&stdout_flush_mutex);
        fprintf(stdout, "%s", msg.text);
        mtx_unlock(&stdout_flush_mutex);
        int last_flush_stdout = atomic_fetch_add(&flushed_stdout_n_times, 1);
        if ((last_flush_stdout + 1) % config.flush_every_n == 0){
            fflush(stdout);
        }
    }
    if (!config.log_to_file || !queue_was_opened) return;
    SYSV(mq_send(log_mq_writer, (const char*)&msg, sizeof(msg), 0), MQ_FAILED, "mq_send");
}

void log_error_and_exit(void (*exit_function)(int), const char *format, ...){
    char buf[MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH];

    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    log_event_type_t fatal_type = (log_role == LOG_ROLE_CLIENT) ? FATAL_ERROR_CLIENT : FATAL_ERROR;
    log_event(NON_APPLICABLE_LOG_ID, fatal_type, "%s", buf);
    

    if (exit_function) {
        // prima la exit funztion, che potrebbe voler loggare
        // ed ha la responsabilità di chiamare log_close prima di uscire
        exit_function(EXIT_FAILURE);
    } else {
        log_close();
        exit(EXIT_FAILURE);
    }
}

void log_fatal_error(char *format, ...) {
    char buf[MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH];
    va_list ap; 
	va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

	// se siamo nel client vogliamo far terminare il client ma non il server, nè il logging
	// per questo FATAL_ERROR_CLIENT non è un evento terminatore (vedi la log event types lookup table)
	log_event_type_t fatal_type = (log_role == LOG_ROLE_CLIENT) ? FATAL_ERROR_CLIENT : FATAL_ERROR;
	
    log_event(NON_APPLICABLE_LOG_ID, fatal_type, "%s", buf);
	log_close(); // chiude il logging in modo ordinato
}

void log_parsing_error(char *format, ...) {
    char buf[MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH];
    va_list ap; 
	va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    log_event(NON_APPLICABLE_LOG_ID, PARSING_ERROR, "%s", buf);
}

void log_register_this_thread(const char *name) {
    if (!name) name = "unnamed thread";
    thrd_t me = thrd_current();
    mtx_lock(&thread_names_mutex);

    // ignoro se già registrato
    for (int i = 0; i < MAX_LOG_THREADS; i++) {
        if (thread_names[i].tid != 0 && thrd_equal(thread_names[i].tid, me)) {
            mtx_unlock(&thread_names_mutex);
            return; // non aggiorna, non fa nulla
        }
    }

    // trovo slot libero
    for (int i = 0; i < MAX_LOG_THREADS; i++) {
        if (thread_names[i].tid == 0) {
            thread_names[i].tid = me;
            snprintf(thread_names[i].name, MAX_THREAD_NAME_LENGTH, "%s", name);
            thread_names[i].name[MAX_THREAD_NAME_LENGTH - 1] = '\0';
            break;
        }
    }

    mtx_unlock(&thread_names_mutex);
}

const char *log_get_current_thread_name(void) {
    thrd_t me = thrd_current();
    mtx_lock(&thread_names_mutex);
    for (int i = 0; i < MAX_LOG_THREADS; i++) {
        if (thread_names[i].tid != 0 && thrd_equal(thread_names[i].tid, me)) {
            const char *res = thread_names[i].name;
            mtx_unlock(&thread_names_mutex);
            return res;
        }
    }
    mtx_unlock(&thread_names_mutex);
    return "unnamed thread";
}
