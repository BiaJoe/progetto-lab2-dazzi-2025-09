#ifndef CLIENT_H
#define CLIENT_H

#define _GNU_SOURCE // per usare getline

#include <unistd.h>
#include "debug.h"
#include "log.h"

#define LOG_IGNORING_ERROR(m) log_event(AUTOMATIC_LOG_ID, WRONG_EMERGENCY_REQUEST_IGNORED_CLIENT, m) 

#define PRINT_CLIENT_USAGE(argv0)  \
	do { \
		printf("Utilizzo: \n"); \
		printf("%s <nome_emergenza> <coord_x> <coord_y> <delay_in_secs>       (inserimento diretto) \n", argv0); \
		printf("%s -f <nome_file>                                             (leggi da file)       \n", argv0); \
	} while(0)

// scorciatoria solo per il main() di client.c
#define DIE(argv0, last_words) \
	do { \
		LOG_IGNORING_ERROR(last_words); \
		PRINT_CLIENT_USAGE(argv0); \
		exit(EXIT_FAILURE); \
	} while(0)

// modalità del client

#define UNDEFINED_MODE -1
#define NORMAL_MODE 1
#define FILE_MODE_STRING "-f"
#define FILE_MODE 2
#define STOP_MODE_STRING "-s"
#define STOP_MODE 99

#define MAX_EMERGENCY_REQUEST_COUNT 256 
#define MAX_EMERGENCY_REQUEST_LENGTH 512

void handle_normal_mode_input(char* args[]);
void handle_file_mode_input(char* args[]);
int  send_emergency_request_message(char *name, char *x_string, char *y_string, char *delay_string);
void handle_stop_mode_client(void);
#endif
