#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include "structs.h"
#include "log.h"
#include "debug.h"
#include "emergency_priority_queue.h"
#include "bresenham.h"

void change_rescuer_digital_twin_destination(rescuer_digital_twin_t *t, int new_x, int new_y);

//funzioni per lockare e unlockare mutex 

void lock_rescuer_types(server_context_t *ctx);
void unlock_rescuer_types(server_context_t *ctx);

void lock_queue(emergency_queue_t *q);
void unlock_queue(emergency_queue_t *q);
void lock_list(emergency_list_t *l);
void unlock_list(emergency_list_t *l);
void lock_node(emergency_node_t *n);
void unlock_node(emergency_node_t *n);

#endif  

