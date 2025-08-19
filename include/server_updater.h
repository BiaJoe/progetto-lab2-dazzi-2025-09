#ifndef SERVER_UPDATER_H
#define SERVER_UPDATER_H

#include "utils.h"
#include "log.h"
#include "bresenham.h"
#include "server_utils.h"
#include "server_clock.h"

int server_updater(void *arg);
void update_rescuers_states_and_positions_on_the_map_logging(server_context_t *ctx);
int update_rescuer_digital_twin_state_and_position_logging(rescuer_digital_twin_t *t, int minx, int miny, int height, int width);
void send_rescuer_digital_twin_back_to_base_logging(rescuer_digital_twin_t *t);
void timeout_emergency_if_needed_logging(emergency_node_t* n);
void update_working_emergency_node_status_signaling_logging(emergency_node_t *n);
void promote_to_medium_priority_if_needed_logging(emergency_queue_t* q, emergency_node_t* n);
void update_working_emergencies_statuses_blocking(server_context_t *ctx);
void update_waiting_emergency_node_status_logging(emergency_node_t *n);
void update_waiting_emergency_statuses_blocking(server_context_t *ctx);
void cancel_all_working_emergencies_signaling(server_context_t *ctx);


#endif