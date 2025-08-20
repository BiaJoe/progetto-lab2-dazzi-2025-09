#include <stdio.h>
#include <stdlib.h> 
#include "debug.h"
#include "parsers.h"
#include "utils.h"

void print_rescuer_types(rescuer_type_t** rescuers, int rescuer_count){
	printf("RESCUER COUNT: %d\n", rescuer_count);
	for(int i = 0; i < rescuer_count; i++){
		printf("rescuer %d: %s, [%d, %d], %d cells/s\n", i, R_NAME(i), R_X(i), R_Y(i), R_SPEED(i));
		for(int j = 0; j < rescuers[i]->amount; j++){
			printf("\t Gemello %d: [%d, %d] %s\n", T_ID(i,j), T_X(i,j), T_Y(i,j), get_status_string(TWIN(i,j)));
		}
	}	
}

char * get_status_string(rescuer_digital_twin_t *rescuer_digital_twin){
	switch(rescuer_digital_twin->status){
		case IDLE:
			return "IDLE";
		case EN_ROUTE_TO_SCENE:
			return "EN_ROUTE_TO_SCENE";
		case ON_SCENE:
			return "ON_SCENE";
		case RETURNING_TO_BASE:
			return "RETURNING_TO_BASE";
		default:
			return "UNKNOWN_STATUS";
	}

}

void print_emergency_types(emergency_type_t** emergency_types, int emergency_count){
	printf("EMERGENCY COUNT: %d\n", emergency_count);
	for(int i = 0; i < emergency_count; i++){
		printf("emergency %d: %s, %d\n", i, E_NAME(i), E_PRIORITY(i));
		for(int j = 0; j < emergency_types[i]->rescuers_req_number; j++){
			printf("rescuer request %d: %s\n", j, get_name_of_rescuer_requested(E_RESCUERS(i,j)));
		}
	}
}

void print_env(char* queue, int height, int width){
	printf("queue: %s \nheight: %d \nwidth: %d\n",queue, height, width);
}