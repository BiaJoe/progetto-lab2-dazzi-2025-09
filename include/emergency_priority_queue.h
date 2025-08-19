#ifndef EMERGENCY_PRIORITY_QUEUE_H
#define EMERGENCY_PRIORITY_QUEUE_H

#include "utils.h"

emergency_t *mallocate_emergency(server_context_t *ctx, char* name, int x, int y, time_t timestamp);
void free_emergency(emergency_t* e);
emergency_node_t* mallocate_emergency_node(emergency_t *e);
void free_emergency_node(emergency_node_t* n);
emergency_list_t *mallocate_emergency_list();
void free_emergency_list(emergency_list_t *list);
emergency_queue_t *mallocate_emergency_queue();
void free_emergency_queue(emergency_queue_t *q);

int is_queue_empty(emergency_queue_t *q);

void append_emergency_node(emergency_list_t* list, emergency_node_t* node);
void push_emergency_node(emergency_list_t* list, emergency_node_t *node);
int is_the_first_node_of_the_list(emergency_node_t* node);
int is_the_last_node_of_the_list(emergency_node_t* node);
void remove_emergency_node_from_its_list(emergency_node_t* node);
void change_emergency_node_list_append(emergency_node_t *n, emergency_list_t *new_list);
void change_emergency_node_list_push(emergency_node_t *n, emergency_list_t *new_list);
emergency_node_t* decapitate_emergency_list(emergency_list_t* list);
void enqueue_emergency_node(emergency_queue_t* q, emergency_node_t *n);
emergency_node_t* dequeue_emergency_node(emergency_queue_t* q);
void change_node_priority_list(emergency_queue_t* q, emergency_node_t* n, int newp);


 

#endif