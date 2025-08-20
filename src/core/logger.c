#include "logger.h"

int compare_log_messages(const void *a, const void *b) {
	const log_message_t *m1 = (const log_message_t*)a;
	const log_message_t *m2 = (const log_message_t*)b;
	return (m1->timestamp > m2->timestamp) - (m1->timestamp < m2->timestamp);
}

int we_should_flush(int logs_so_far){
	return ((logs_so_far % HOW_MANY_LOGS_BEFORE_FLUSHING_THE_STREAM) == 0) ? YES : NO;
}

void sort_write_flush_log_buffer(log_message_t *log_buffer, int *buffer_len, FILE *log_file) {
	qsort(log_buffer, *buffer_len, sizeof(log_message_t), compare_log_messages);
	for (int i = 0; i < *buffer_len; ++i) {
		fprintf(log_file, "%s", log_buffer[i].message);
	}
	fflush(log_file);
	*buffer_len = 0;
}

// Processo che riceve messaggi da log.c con message queue e li logga con 
void logger(void){
	FILE *log_file = fopen(LOG_FILE, "a");
	check_error_fopen(log_file); 

	mqd_t mq;

	mq = mq_open(LOG_QUEUE_NAME, O_RDWR);
	check_error_mq_open(mq);

	log_message_t log_buffer[LOG_BUFFER_CAPACITY];
	int buffer_len = 0;	
	int logs_so_far = 0;

	// ricevo i messaggi
	// se arriva il messaggio di stop esco perché per qualche motivo ho finito
	// altrimenti li scrivo nel logfile
	while (1) {
		log_message_t msg;
		ssize_t bytes_arrived = mq_receive(mq, (char*)&msg, sizeof(msg), NULL);
		check_error_mq_receive(bytes_arrived);

		// se è il messaggio di uscita scrivo i messaggi che sono rimasti nel buffer ed esco
		if(strcmp(msg.message, STOP_LOGGING_MESSAGE) == 0){
			sort_write_flush_log_buffer(log_buffer, &buffer_len, log_file);
			break;
		}

		// Aggiungo al buffer
		log_buffer[buffer_len] = msg;
		buffer_len++;

		// se il buffer è pieno o è il momento di scrivere
		// scrivo i messaggi del buffer
		if (buffer_len >= LOG_BUFFER_CAPACITY || we_should_flush(++logs_so_far)) 
			sort_write_flush_log_buffer(log_buffer, &buffer_len, log_file);

	}

	mq_close(mq);
	fclose(log_file);
	exit(EXIT_SUCCESS);
}

