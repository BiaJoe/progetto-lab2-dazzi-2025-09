#ifndef EMERGENCY_REQUESTS_RECIEVER_H
#define EMERGENCY_REQUESTS_RECIEVER_H

#include "server_utils.h"

int emergency_requests_reciever(server_context_t *ctx);
int parse_emergency_request(char *message, char* name, int *x, int *y, time_t *timestamp);
int emergency_request_values_are_illegal(server_context_t *ctx, char* name, int x, int y, time_t timestamp);

#endif
