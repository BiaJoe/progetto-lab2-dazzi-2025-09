#include "server.h"

int thread_worker(void *arg){
	server_context_t *ctx = arg;

	while(!ctx->server_must_stop){		

	}		

	return 0;
}
