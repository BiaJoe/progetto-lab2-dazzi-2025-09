#include "client.h"
#include "config_client.h"

// client.c prende in input le emergenze e fa un minimo di error handling
// poi invia le emergenze, ma lascia al server il compito di finire il check dei valori
mqd_t mq;
char requests_argument_separator;

static void close_client(int exit_code){
	switch (exit_code) {
		case EXIT_SUCCESS:
			log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "Il lavoro del client è finito. Il processo si chiude");
			break;
		case EXIT_FAILURE:
			log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "Un errore ha interrotto il lavore del Client. Il processo si chiude");
			break;
		default:
			log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "Il CLient si chiude con codice sconosciuto");
			exit_code = EXIT_FAILURE;
			break;
	}
	if(mq != (mqd_t)-1) mq_close(mq);
	log_close();
	exit(exit_code);
}

int main(int argc, char* argv[]){
	// inizio a loggare lato client
	log_init(LOG_ROLE_CLIENT, client_logging_config);
	log_event(NON_APPLICABLE_LOG_ID, CLIENT, "inizia il processo del client");

	// capisco in quale modalità mi trovo
	// in questo caso le modalità sono 2 + undefined, 
	// ma non sarebbe difficile espandere il codice ed aggiungerne di più in futuro
	client_mode_t mode = UNDEFINED_MODE;

	switch (argc) {
		case 5: 
			mode = NORMAL_MODE;  
			break;	
		case 3: 
			if (strcmp(argv[1], FILE_MODE_STRING) == 0) mode = FILE_MODE;
			break; // espandibile
		case 2: 
			if (strcmp(argv[1], STOP_SERVER_AND_CLIENT_MODE_STRING) == 0) mode = STOP_CLIENT_AND_SERVER_MODE;
			if (strcmp(argv[1], STOP_MODE_STRING) == 0) mode = STOP_JUST_CLIENT_MODE;
			if (strcmp(argv[1], HELP_MODE_STRING) == 0) mode = HELP_MODE;
			break; // espandibile
		default: 
			DIE(argv[0], "modalità di inserimento dati non riconosciuta", close_client(EXIT_FAILURE));
			break; 
	}

	sem_t *sem;
	int shared_memory_fd;
	client_server_shm_t *shm_data;
	config_files_names_t config;

	SYSV(sem = sem_open(SEM_NAME, 0), SEM_FAILED, "sem_open");
    SYSC(sem_wait(sem), "sem_wait");
	SYSC(sem_post(sem), "sem_post"); // ripostoi il semaforo per altri client ipotetici
    SYSC(sem_close(sem), "sem_close");

    SYSC(shared_memory_fd = shm_open(SHM_NAME, O_RDONLY, 0), "shm_open");
    SYSV(shm_data = mmap(NULL, sizeof(*shm_data), PROT_READ, MAP_SHARED, shared_memory_fd, 0), MAP_FAILED, "mmap");
	SYSC(close(shared_memory_fd), "close");

	SYSV(mq = mq_open(shm_data->queue_name, O_WRONLY), MQ_FAILED, "mq_open");
    requests_argument_separator = shm_data->requests_argument_separator;
	snprintf(config.env, FILENAME_BUFF, "%s", shm_data->config.env);
	snprintf(config.rescuer_types, FILENAME_BUFF, "%s", shm_data->config.rescuer_types);
	snprintf(config.emergency_types, FILENAME_BUFF, "%s", shm_data->config.emergency_types);

	SYSC(munmap(shm_data, sizeof(*shm_data)), "munmap");

	switch (mode) {
		case HELP_MODE:						print_help_info(argv, config); break;
		case NORMAL_MODE: 					handle_normal_mode_input(argv); break;
		case FILE_MODE:  					handle_file_mode_input(argv); break;
		case STOP_CLIENT_AND_SERVER_MODE: 	handle_stop_mode_client_server(); break;
		case STOP_JUST_CLIENT_MODE:			break; // il client non ha niente da fare
		default: 		  					DIE(argv[0], "modalità di inserimento dati non valida", close_client(EXIT_FAILURE)); break;
	}

	// a questo punto l'emergenza o le emergenze sono state inviate.
	// oppure il client ha detto al server di fermarsi
	close_client(EXIT_SUCCESS);
	return 0;
}

static void cat_file(FILE *f, int max_file_lines, int max_file_line_length) {
    if (!f) return;

    char buffer[max_file_line_length];
    int line_count = 0;

    while (line_count < max_file_lines && fgets(buffer, sizeof(buffer), f)) {
        fputs(buffer, stdout); 
        line_count++;
    }
	printf("\n\n");
	fflush(stdout);
}

void print_help_info(char* argv[], config_files_names_t config) {
	FILE *envp, *resp, *emep;
	
	envp = fopen(config.env, "r");
	resp = fopen(config.rescuer_types, "r");
	emep = fopen(config.emergency_types, "r");

	check_error_fopen(envp);
	check_error_fopen(resp);
	check_error_fopen(emep);

	PRINT_CLIENT_USAGE(argv[0]);

	printf("\n##########    FILE DI CONFIGURAZIONE   ##########  \n\n"); 

	cat_file(envp, MAX_ENV_CONF_LINES, MAX_ENV_CONF_LINE_LENGTH);
	cat_file(resp, MAX_RESCUER_CONF_LINES, MAX_RESCUER_CONF_LINE_LENGTH);
	cat_file(emep, MAX_EMERGENCY_CONF_LINES, MAX_EMERGENCY_CONF_LINE_LENGTH);

	if (envp) fclose(envp);
    if (resp) fclose(resp);
    if (emep) fclose(emep);
}

void handle_stop_mode_client_server(){
	char *buffer = MQ_STOP_MESSAGE;
	SYSC(mq_send(mq, buffer, strlen(buffer) + 1, 0), "mq_send");
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "inviato messaggio di stop dal client");
}

void send_emergency_request_message(char string[MAX_EMERGENCY_REQUEST_LENGTH + 1]) {
	// il client si limita a spedire l'emergenza, il server fa tutti i controlli
	// il client deve estrarre almeono il delay e aspettare quel tanto prima di inviare l'emergenza
	unsigned int delay = 1;
	if (sscanf(extract_last_token(string, requests_argument_separator), "%ud", &delay) != 1){
		LOG_IGNORING_ERROR("delay non trovato!");
		return;
	}
	if (delay > MAX_REQUEST_DELAY_SECONDS) {
		LOG_IGNORING_ERROR("delay troppo lungo! Si intende per caso fare un dispetto e bloccare il sistema?? 😡");
		return;
	}
	sleep(delay); 		// aspetto il delay prima di inviare l'emergenza, in modo da poterla subito processare nel server senza dover aspettare
	SYSC(mq_send(mq, string, strlen(string) + 1, 0), "mq_send");
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "inviata emergenza al server");
}

// gestisce la singola emergenza passata da terminale
void handle_normal_mode_input(char* args[]){
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "avvio della modalità di inserimento diretta");

	if(strlen(args[1]) + strlen(args[2]) + strlen(args[3]) + strlen(args[4]) + 3 > MAX_EMERGENCY_REQUEST_LENGTH){ // considero anche i 3 spazi
		DIE(args[0], "Richiesta di emergenza troppo lunga", close_client(EXIT_FAILURE));
		return;
	}

	char string[MAX_EMERGENCY_REQUEST_LENGTH + 1];
	int n = snprintf(string, sizeof(string), "%s %s %s %s", args[1], args[2], args[3], args[4]);
	if (n < 0 || n >= (int)sizeof(string)) { 
		DIE(args[0], "Formato richiesta non valido", close_client(EXIT_FAILURE)); 
		return; 
	}

	send_emergency_request_message(string);
}

// gestisce l'emergenza passata per file inviando un'emergenza alla volta riga per riga    
void handle_file_mode_input(char* args[]){
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "avvio della modalità di inserimento da file");
	FILE* emergency_requests_file = fopen(args[2], "r");
	if(!emergency_requests_file) DIE(args[0], "file non trovato! :(", close_client(EXIT_FAILURE));

	check_error_fopen(emergency_requests_file);

	char *line = NULL;
	size_t len = 0;
	int line_count = 0, emergency_count = 0;
	char buf[MAX_EMERGENCY_REQUEST_LENGTH + 1];

	while (getline(&line, &len, emergency_requests_file) != -1) {
		line_count++;

		if(line_count > MAX_CLIENT_INPUT_FILE_LINES)
			log_error_and_exit(close_client, "linee massime superate nel client. Interruzione della lettura emergenze");
		if(emergency_count > MAX_EMERGENCY_REQUEST_COUNT)
			log_error_and_exit(close_client,"numero di emergenze richieste massime superate nel client. Interruzione della lettura emergenze");

		strncpy(buf, line, sizeof(buf)); 
		buf[sizeof(buf) - 1] = '\0';
		send_emergency_request_message(buf);

		emergency_count++;
	}

	free(line);
	fclose(emergency_requests_file);

	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "Invio di emergency_request da file %s completato: %d emergenze lette, %d inviate", args[2], line_count, emergency_count);
}

