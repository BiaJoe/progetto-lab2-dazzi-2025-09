#ifndef SERVER_H
#define SERVER_H

#include <signal.h>
#include <sys/wait.h>
#include "log.h"
#include "logger.h"
#include "parsers.h"
#include "server_utils.h"
#include "server_clock.h"
#include "server_updater.h"
#include "server_emergency_handler.h"
#include "emergency_requests_reciever.h"

#define THREAD_POOL_SIZE 10

#define LOG_IGNORE_EMERGENCY_REQUEST(m) log_event(NO_ID, WRONG_EMERGENCY_REQUEST_IGNORED_SERVER, m)
#define LOG_EMERGENCY_REQUEST_RECIVED() log_event(NO_ID, EMERGENCY_REQUEST_RECEIVED, "emergenza ricevuta e messa in attesa di essere processata!")


// server.c
void funzione_per_sigchild(int sig);
void server(void);
void close_server(server_context_t *ctx);
server_context_t *mallocate_server_context();
void cleanup_server_context(server_context_t *ctx);


#endif
