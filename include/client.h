#ifndef CLIENT_H
#define CLIENT_H

#define _GNU_SOURCE // per usare getline

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <mqueue.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>


#include "log.h"
#include "utils.h"

#define MAX_EMERGENCY_REQUEST_COUNT  256 
#define MAX_EMERGENCY_REQUEST_LENGTH 512
#define MAX_CLIENT_INPUT_FILE_LINES  128
#define MAX_REQUEST_DELAY_SECONDS    32

#define LOG_IGNORING_ERROR(m) log_event(AUTOMATIC_LOG_ID, WRONG_EMERGENCY_REQUEST_IGNORED_CLIENT, m) 

#define PRINT_CLIENT_USAGE(argv0)  \
	do { \
		printf("Utilizzo: \n"); \
		printf("%s <nome_emergenza> <coord_x> <coord_y> <delay_in_secs> (inserimento diretto) \n", argv0); \
		printf("%s -f <nome_file>                                       (leggi da file)       \n", argv0); \
	} while(0)

// scorciatoria solo per il main() di client.c
#define DIE(argv0, last_words) \
	do { \
		LOG_IGNORING_ERROR(last_words); \
		PRINT_CLIENT_USAGE(argv0); \
		exit(EXIT_FAILURE); \
	} while(0)

// modalità del client

typedef enum {
	UNDEFINED_MODE,
	NORMAL_MODE,
	FILE_MODE,
	STOP_JUST_CLIENT_MODE,
	STOP_CLIENT_AND_SERVER_MODE,
	NUMBER_OF_CLIENT_MODES,
} client_mode_t;

void handle_normal_mode_input(char* args[]);
void handle_file_mode_input(char* args[]);
void send_emergency_request_message(char string[MAX_EMERGENCY_REQUEST_LENGTH + 1]);
void handle_stop_mode_client_server(void);

#endif
