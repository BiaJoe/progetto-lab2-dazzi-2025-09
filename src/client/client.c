#include "client.h"


// client.c prende in input le emergenze e fa un minimo di error handling
// poi invia le emergenze, ma lascia al server il compito di finire il check dei valori
mqd_t mq;

int main(int argc, char* argv[]){
	// inizio a loggare lato client
	log_init(LOG_ROLE_CLIENT);

	// controllo il numero di argomenti
	if(argc != 3 && argc != 5 && argc != 2) 
		DIE(argv[0], "numero di argomenti dati al programma sbagliato");

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
			if (strcmp(argv[1], STOP_MODE_STRING) == 0) mode = STOP_MODE;
			break; // espandibile
		default: 
			DIE(argv[0], "modalità di inserimento dati non riconosciuta");
			break; // non dovrebbe mai arrivarci
	}


	sem_t *sem;
	int shared_memory_fd;
	client_server_shm_t *shm_data;

	SYSV(sem = sem_open(SEM_NAME, 0), SEM_FAILED, "sem_open");
    SYSC(sem_wait(sem), "sem_wait");
	SYSC(sem_post(sem), "sem_post"); // ripostoi il semaforo per altri client ipotetici
    SYSC(sem_close(sem), "sem_close");
    SYSC(shared_memory_fd = shm_open(SHM_NAME, O_RDONLY, 0), "shm_open");
    SYSV(shm_data = mmap(NULL, sizeof(*shm_data), PROT_READ, MAP_SHARED, shared_memory_fd, 0), MAP_FAILED, "mmap");
	SYSC(close(shared_memory_fd), "close");
	SYSV(mq = mq_open(shm_data->queue_name, O_WRONLY), MQ_FAILED, "mq_open");
    SYSC(munmap(shm_data, sizeof(*shm_data)), "munmap");

	

	switch (mode) {
		case NORMAL_MODE: handle_normal_mode_input(argv); break;
		case FILE_MODE:   handle_file_mode_input(argv); break;
		case STOP_MODE:   handle_stop_mode_client(); break;
		default: 		  DIE(argv[0], "modalità di inserimento dati non valida"); break;
	}

	// a questo punto l'emergenza o le emergenze sono state inviate.
	// sono tutte emergenze SINTATTICAMENTE valide
	// spetta al server controllare la SEMANTICA (se i valori x,y,timestamo sono nei limiti)
	// se non lo sono l'emergenza viene ignorata nel server
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "il lavoro del client è finito. Il processo si chiude");
	mq_close(mq);
	log_close();
	return 0;
}


void handle_stop_mode_client(){
	char *buffer = STOP_MESSAGE_FROM_CLIENT;
	SYSC(mq_send(mq, buffer, strlen(buffer) + 1, 0), "mq_send");
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "inviato messaggio di stop dal client");
}

// prende nome, coordinate e timestamp in forma di stringhe, fa qualche controllo e li spedisce
// ritorna 0 se fallisce senza spedire, 1 se riesce
int send_emergency_request_message(char *name, char *x_string, char *y_string, char *delay_string) {
	errno = 0; // my_atoi usa errno
	int x = my_atoi(x_string);
	int y = my_atoi(y_string);
	int d = my_atoi(delay_string);
	d = ABS(d); // per sicurezza faccio il valore assoluto

	char buffer[MAX_EMERGENCY_REQUEST_LENGTH + 1];

	if(errno != 0){
		LOG_IGNORING_ERROR("caratteri non numerici presenti nei valori numerici dell'emergenza richiesta");
		return 0;
	}

	if(strlen(name) >= MAX_EMERGENCY_NAME_LENGTH){
		LOG_IGNORING_ERROR("nome emergenza troppo lungo");
		return 0;
	}

	if(snprintf(buffer, sizeof(buffer), "%s %d %d %d", name, x, y, d) >= MAX_EMERGENCY_REQUEST_LENGTH + 1){
		LOG_IGNORING_ERROR("messaggio di emergenza troppo lungo");
		return 0;
	}

	// aspetto il delay prima di inviare l'emergenza, in modo da poterla subito processare nel server senza dover aspettare
	sleep(d);

	// il client garantisce la correttezza sintattica della richiesta
	// al server spetta controllare la correttezza semantica e processare la richiesta
	SYSC(mq_send(mq, buffer, strlen(buffer) + 1, 0), "mq_send");
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "inviata emergenza al server");

	return 1;
}

// gestisce la singola emergenza passata da terminale
void handle_normal_mode_input(char* args[]){
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "avvio della modalità di inserimento diretta");
	send_emergency_request_message(args[1], args[2], args[3], args[4]);
}

// gestisce l'emergenza passata per file inviando un'emergenza alla volta riga per riga    
void handle_file_mode_input(char* args[]){
	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "avvio della modalità di inserimento da file");
	FILE* emergency_requests_file = fopen(args[2], "r");
	if(!emergency_requests_file) DIE(args[0], "file non trovato! :(");

	check_error_fopen(emergency_requests_file);

	char *name, *x, *y, *d, *must_be_null;

	char *line = NULL;
	size_t len = 0;
	int line_count = 0, emergency_count = 0;

	while (getline(&line, &len, emergency_requests_file) != -1) {
		line_count++;

		if(line_count > MAX_CLIENT_INPUT_FILE_LINES)
			log_fatal_error("linee massime superate nel client. Interruzione della lettura emergenze");
		if(emergency_count > MAX_EMERGENCY_REQUEST_COUNT)
			log_fatal_error("numero di emergenze richieste massime superate nel client. Interruzione della lettura emergenze");
		
		name 			= strtok(line, EMERGENCY_REQUEST_ARGUMENT_SEPARATOR);
		x 				= strtok(NULL, EMERGENCY_REQUEST_ARGUMENT_SEPARATOR);
		y 				= strtok(NULL, EMERGENCY_REQUEST_ARGUMENT_SEPARATOR);
		d 				= strtok(NULL, EMERGENCY_REQUEST_ARGUMENT_SEPARATOR);
		must_be_null 	= strtok(NULL, EMERGENCY_REQUEST_ARGUMENT_SEPARATOR);

		if(name == NULL || x == NULL || y == NULL || d == NULL || must_be_null != NULL){
			LOG_IGNORING_ERROR("riga del file di emergenze sbagliata");
			continue;
		}
		
		if(!send_emergency_request_message(name, x, y, d))
			continue;

		emergency_count++;
	}

	free(line);
	fclose(emergency_requests_file);

	log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "Invio di emergency_request da file %s completato: %d emergenze lette, %d inviate", args[2], line_count, emergency_count);
}

