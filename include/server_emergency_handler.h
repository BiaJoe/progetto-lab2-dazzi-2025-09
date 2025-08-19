#ifndef SERVER_EMERGENCY_HANDLER_H
#define SERVER_EMERGENCY_HANDLER_H

#include "server_utils.h"

#define RESCUER_SEARCHING_FAIR_MODE 'f'
#define RESCUER_SEARCHING_STEAL_MODE 's'
#define TIME_INTERVAL_BETWEEN_RESCUERS_SEARCH_ATTEMPTS_SECONDS 3

int server_emergency_handler(void *arg);

int handle_search_for_rescuers(server_context_t *ctx, emergency_node_t *n);
int handle_waiting_for_rescuers_to_arrive(server_context_t *ctx, emergency_node_t *n);
int handle_emergency_processing(server_context_t *ctx, emergency_node_t *n);

void cancel_and_unlock_working_node_blocking(server_context_t *ctx, emergency_node_t *n);
void pause_and_unlock_working_node_blocking(server_context_t *ctx, emergency_node_t *n);
void move_working_node_to_completed_blocking(server_context_t *ctx, emergency_node_t *n);
void send_rescuer_digital_twin_to_scene_logging(rescuer_digital_twin_t *t, emergency_node_t *n);
int is_rescuer_digital_twin_available(rescuer_digital_twin_t *dt);

int is_rescuer_digital_twin_already_chosen(rescuer_digital_twin_t *dt, rescuer_digital_twin_t **chosen, int count);
rescuer_digital_twin_t *find_nearest_available_rescuer_digital_twin(rescuer_type_t *r, emergency_node_t *n, rescuer_digital_twin_t **chosen, int count);
int is_rescuer_digital_twin_available(rescuer_digital_twin_t *dt);
int rescuer_digital_twin_must_be_stolen(rescuer_digital_twin_t *dt);
rescuer_digital_twin_t *try_to_find_nearest_rescuer_from_less_important_emergency(rescuer_type_t *r, emergency_node_t *n, rescuer_digital_twin_t **chosen, int count);
void pause_emergency_blocking_signaling_logging(emergency_node_t *n);
int find_and_send_nearest_rescuers(emergency_node_t *n, char mode);



#endif
