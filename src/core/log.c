#include "log.h"

static log_role_t  log_role;
static mqd_t       log_mq_reader = (mqd_t)-1;   // server: riceve
static mqd_t       log_mq_writer = (mqd_t)-1;   // client/server/altro: scrive
static thrd_t      logger_thread_id;
static atomic_bool logger_running = false;

#if LOG_TO_FILE
static FILE *log_fp = NULL;
#endif

static unsigned long long log_event_counters[LOG_EVENT_TYPES_COUNT];

//lookup table per ogni evento
static const log_event_info_t LOG_EVENT_LOOKUP_TABLE[LOG_EVENT_TYPES_COUNT] = {
//  TYPE											// STRING										// CODICE			//TERMINA?		// DA LOGGARE?
    [NON_APPLICABLE]                    		= { "NON_APPLICABLE",                   			"N/A ",  			false, 			true },
    [DEBUG]                             		= { "DEBUG",                            			"DEH ",  			false, 			true },
    [FATAL_ERROR]                       		= { "FATAL_ERROR",                      			"ferr",  			true, 			true },
    [FATAL_ERROR_CLIENT]                       	= { "FATAL_ERROR_CLIENT",                      		"ferc",  			false, 			true },
	[FATAL_ERROR_PARSING]               		= { "FATAL_ERROR_PARSING",              			"fepa",  			true, 			true },
    [FATAL_ERROR_LOGGING]               		= { "FATAL_ERROR_LOGGING",              			"felo",  			true, 			true },
    [FATAL_ERROR_MEMORY]                		= { "FATAL_ERROR_MEMORY",               			"feme",  			true, 			true },
    [FATAL_ERROR_FILE_OPENING]          		= { "FATAL_ERROR_FILE_OPENING",         			"fefo",  			true, 			true },
    [EMPTY_CONF_LINE_IGNORED]           		= { "EMPTY_CONF_LINE_IGNORED",          			"ecli",  			false, 			true },
    [DUPLICATE_RESCUER_REQUEST_IGNORED] 		= { "DUPLICATE_RESCUER_REQUEST_IGNORED",			"drri",  			false, 			true },
    [WRONG_RESCUER_REQUEST_IGNORED]     		= { "WRONG_RESCUER_REQUEST_IGNORED",    			"wrri",  			false, 			true },
    [DUPLICATE_EMERGENCY_TYPE_IGNORED]  		= { "DUPLICATE_EMERGENCY_TYPE_IGNORED", 			"deti",  			false, 			true },
    [DUPLICATE_RESCUER_TYPE_IGNORED]    		= { "DUPLICATE_RESCUER_TYPE_IGNORED",   			"drti",  			false, 			true },
    [WRONG_EMERGENCY_REQUEST_IGNORED_CLIENT] 	= { "WRONG_EMERGENCY_REQUEST_IGNORED_CLIENT",		"werc", 			false, 			true },
    [WRONG_EMERGENCY_REQUEST_IGNORED_SERVER] 	= { "WRONG_EMERGENCY_REQUEST_IGNORED_SERVER",		"wers", 			false, 			true },
    [LOGGING_STARTED]                   		= { "LOGGING_STARTED",                  			"lsta",  			false,  		true },
    [LOGGING_ENDED]                     		= { "LOGGING_ENDED",                    			"lend",  			true,  			true },
    [PARSING_STARTED]                   		= { "PARSING_STARTED",                  			"psta",  			false,  		true },
    [PARSING_ENDED]                     		= { "PARSING_ENDED",                    			"pend",  			false,  		true },
    [RESCUER_TYPE_PARSED]               		= { "RESCUER_TYPE_PARSED",              			"rtpa",  			false,  		true },
    [RESCUER_DIGITAL_TWIN_ADDED]        		= { "RESCUER_DIGITAL_TWIN_ADDED",       			"rdta",  			false,  		true },
    [EMERGENCY_PARSED]                  		= { "EMERGENCY_PARSED",                 			"empa",  			false,  		true },
    [RESCUER_REQUEST_ADDED]             		= { "RESCUER_REQUEST_ADDED",            			"rrad",  			false,  		true },
    [SERVER_UPDATE]                     		= { "SERVER_UPDATE",                    			"seup",  			false,  		false },
    [SERVER]                            		= { "SERVER",                           			"srvr",  			false,  		true },
    [CLIENT]                            		= { "CLIENT",                           			"clnt",  			false,  		true },
    [EMERGENCY_REQUEST_RECEIVED]        		= { "EMERGENCY_REQUEST_RECEIVED",       			"errr",  			false,  		true },
    [EMERGENCY_REQUEST_PROCESSED]       		= { "EMERGENCY_REQUEST_PROCESSED",      			"erpr",  			false,  		true },
    [MESSAGE_QUEUE_CLIENT]              		= { "MESSAGE_QUEUE_CLIENT",             			"mqcl",  			false,  		true },
    [MESSAGE_QUEUE_SERVER]              		= { "MESSAGE_QUEUE_SERVER",             			"mqse",  			false,  		true },
    [EMERGENCY_STATUS]                  		= { "EMERGENCY_STATUS",                 			"esta",  			false,  		true },
    [RESCUER_STATUS]                    		= { "RESCUER_STATUS",                   			"rsta",  			false,  		true },
    [RESCUER_TRAVELLING_STATUS]         		= { "RESCUER_TRAVELLING_STATUS",        			"rtst",  			false,  		true },
    [EMERGENCY_REQUEST]                 		= { "EMERGENCY_REQUEST",                			"erre",  			false,  		true },
    [PROGRAM_ENDED_SUCCESSFULLY]        		= { "PROGRAM_ENDED_SUCCESSFULLY",       			"pesu",  			true, 			true }
};			

const log_event_info_t* get_log_event_info(log_event_type_t e) {
    if (e < 0 || e >= LOG_EVENT_TYPES_COUNT) return &LOG_EVENT_LOOKUP_TABLE[NON_APPLICABLE];
    return &LOG_EVENT_LOOKUP_TABLE[e];
}

// funzione per scrivere una riga di log
static void logger_write_line(char id[MAX_LOG_EVENT_ID_STRING_LENGTH], log_event_type_t evt, const char* text) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long seconds      = ts.tv_sec;
    long milliseconds = ts.tv_nsec / 1000000; // da ns a ms
    char timestamp[32];

    snprintf(timestamp, sizeof(timestamp), "%ld%03ld", seconds, milliseconds);
    const log_event_info_t *info = get_log_event_info(evt);
    const char *ename = info ? info->name : "NOME_NON_TROVATO";
    // size_t tlen = strnlen(text, MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH);

    char message[MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH];
    strncpy(message, text, sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
    // -------------------------

    #if LOG_TO_FILE
    if (log_fp) fprintf(log_fp, LOG_EVENT_STRING_SYNTAX, timestamp, id, ename, message);
    #endif

    #if LOG_TO_STDOUT
    fprintf(stdout, LOG_EVENT_STRING_SYNTAX, timestamp, id, ename, message);
    #endif
}

// thread che legge i messaggi dalla coda e li scrive nel file di log e/o in stdout
// usato solo dal server
static int logger_thread(void *arg) {
    (void)arg;

    #if LOG_TO_FILE
	log_fp = fopen(LOG_FILE, "a");
	check_error_fopen(log_fp);
    #endif

    int lines_logged_since_last_flush = 0;

    while (atomic_load(&logger_running)) {
		log_message_t m;
		ssize_t n = mq_receive(log_mq_reader, (char*)&m, sizeof(m), NULL);
		check_error_mq_receive(n);
        m.text[sizeof(m.text) - 1] = '\0';

		const log_event_info_t *info = get_log_event_info(m.event_type);
		if (!info || info->is_to_log == 0) { continue; }

		// prepara l'ID come stringa includendo i casi particolari
        int idx = (m.event_type >= 0 && m.event_type < LOG_EVENT_TYPES_COUNT) ? m.event_type : NON_APPLICABLE;
		char out_id[MAX_LOG_EVENT_ID_STRING_LENGTH];
		switch (m.id) {
            case AUTOMATIC_LOG_ID:
                snprintf(out_id, sizeof(out_id), "%d", (int)(++log_event_counters[idx]));
                break;
            case NON_APPLICABLE_LOG_ID:
                snprintf(out_id, sizeof(out_id), "%s", NON_APPLICABLE_LOG_ID_STRING);
                break;
            default: 
                snprintf(out_id, sizeof(out_id), "%d", m.id);
                break;
		}

		// scrivi la riga nel log
		logger_write_line(out_id, m.event_type, m.text);
		lines_logged_since_last_flush++;
		if (info->is_terminating) 
            atomic_store(&logger_running, false);
		if (lines_logged_since_last_flush >= LOG_FLUSH_EVERY_N) {
            #if LOG_TO_FILE
       		if (log_fp) fflush(log_fp);
            #endif

            #if LOG_TO_STDOUT
        	fflush(stdout);
            #endif

        	lines_logged_since_last_flush = 0;
    	}
	}

    #if LOG_TO_FILE
	if (log_fp) {
		fflush(log_fp); 
		fclose(log_fp); 
		log_fp = NULL; 
	}
    #endif

    #if LOG_TO_STDOUT
	fflush(stdout);
    #endif

    return 0;
}

// inizializza il sistema di logging
// se siamo nel server il ruolo è di ricevere e anche di scrivere messaggi
// e anche di creare il thread che li scriverà nel logfile o in stdout
// altrimenti solo di scrivere
int log_init(log_role_t role) {
    log_role = role;
    atomic_store(&logger_running, true);

    if (role == LOG_ROLE_SERVER) {
        struct mq_attr attr = {0};
        attr.mq_flags   = 0;
        attr.mq_maxmsg  = MAX_LOG_QUEUE_MESSAGES;
        attr.mq_msgsize = sizeof(log_message_t);

        mq_unlink(LOG_QUEUE_NAME);
        log_mq_reader = mq_open(LOG_QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
        check_error_mq_open(log_mq_reader);

        log_mq_writer = mq_open(LOG_QUEUE_NAME, O_WRONLY);
        check_error_mq_open(log_mq_writer);

        check_error_thread_create(thrd_create(&logger_thread_id, logger_thread, NULL));
        log_event(AUTOMATIC_LOG_ID, LOGGING_STARTED, "logging avviato");
        return 0;
    } 
	// non siamo nel server
	log_mq_writer = mq_open(LOG_QUEUE_NAME, O_WRONLY);
	if (log_mq_writer == (mqd_t)-1) {
		for (int i = 0; i < LOG_QUEUE_OPEN_MAX_RETRIES && log_mq_writer == (mqd_t)-1; ++i) {
			struct timespec t = { .tv_sec = 0, .tv_nsec = LOG_QUEUE_OPEN_RETRY_INTERVAL_NS };
			thrd_sleep(&t, NULL);
			log_mq_writer = mq_open(LOG_QUEUE_NAME, O_WRONLY);
		}
		check_error_mq_open(log_mq_writer);
	}
	return 0;
}


// chiude il sistema di logging
// se siamo nel server chiude il thread che scrive i log e la coda di logging
// se siamo nel client chiude solo la coda di logging
// LOGGA la chiusura, serve per svegloare la mq_receive nel thread logger
void log_close(void) {
    if (log_role == LOG_ROLE_CLIENT) {
        if (log_mq_writer != (mqd_t)-1) mq_close(log_mq_writer);
        return;
    }
	// siamo nel server

	log_event(AUTOMATIC_LOG_ID, LOGGING_ENDED, "logging terminato");
    atomic_store(&logger_running, false);
    thrd_join(logger_thread_id, NULL);

    if (log_mq_writer != (mqd_t)-1) { 
		mq_close(log_mq_writer); 
		log_mq_writer = (mqd_t)-1; 
	}

    if (log_mq_reader != (mqd_t)-1) {
        mq_close(log_mq_reader);
        mq_unlink(LOG_QUEUE_NAME);
        log_mq_reader = (mqd_t)-1;
    }
}

// ho imparato come usare variable argument lists sul libro: The C Programming Language, 2nd Edition
void log_event(int id, log_event_type_t event_type, char *format, ...) {
    const log_event_info_t *info = get_log_event_info(event_type);
    if (!info || !info->is_to_log) return;

    log_message_t msg = { 0 };
    msg.id = id;
    msg.event_type = event_type;

    va_list ap;
    va_start(ap, format);
    vsnprintf(msg.text, MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH, format, ap);
    va_end(ap);
    msg.text[sizeof(msg.text) - 1] = '\0'; // mi assicuro che l'ultimo carattere sia il terminatore
   

    mq_send(log_mq_writer, (const char*)&msg, sizeof(msg), 0);
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
	
	// scrive anche su stderr
    log_event(NON_APPLICABLE_LOG_ID, fatal_type, "%s", buf);
	log_close(); // chiude il logging in modo ordinato
    perror(buf);
    exit(EXIT_FAILURE);
}
