#include <structs.h>


// funzioni per accedere a strutture

char* get_name_of_rescuer_requested(rescuer_request_t *rescuer_request){
	rescuer_type_t *rescuer_type = (rescuer_type_t *)rescuer_request->type;
	return rescuer_type->rescuer_type_name;
}

rescuer_type_t * get_rescuer_type_by_name(char *name, rescuer_type_t **rescuer_types){
	trim_right(name);
	for(int i = 0; rescuer_types[i] != NULL; i++){
		if(strcmp(rescuer_types[i]->rescuer_type_name, name) == 0){
			return rescuer_types[i];
		}
	}
	return NULL;
}

emergency_type_t * get_emergency_type_by_name(char *name, emergency_type_t **emergency_types){
	trim_right(name);
	for(int i = 0; emergency_types[i] != NULL; i++)
		if(strcmp(emergency_types[i]->emergency_desc, name) == 0)
			return emergency_types[i];
	return NULL;
}

rescuer_request_t * get_rescuer_request_by_name(char *name, rescuer_request_t **rescuers){
	for(int i = 0; rescuers[i] != NULL; i++)
		if(strcmp(get_name_of_rescuer_requested(rescuers[i]), name) == 0)
			return rescuers[i];
	return NULL;
}

int get_twin_index_by_id(int id, rescuer_digital_twin_t **list, int amount) {
	for (int i = 0; i < amount; i++) {
		if (list[i]->id == id) {
			return i;
		}
	}
	return -1;
}

// funzioni per liberare strutture importanti

void free_rescuers(rescuers_t *rescuers){
	mtx_destroy(&(rescuers->mutex));
	free_rescuer_types(rescuers->types);
	rescuers->types = NULL;
	free(rescuers);
}

void free_rescuer_types(rescuer_type_t **rescuer_types){
	for(int i = 0; rescuer_types[i] != NULL; i++){
		free(rescuer_types[i]->rescuer_type_name);			    //libero il puntatore al nome 
		for(int j = 0; j < rescuer_types[i]->amount; j++){	    // libero ogni gemello digitale
			free(rescuer_types[i]->twins[j]->trajectory);	    // libero la struttura usata per gestire i suoi movimenti
			free(rescuer_types[i]->twins[j]);
		}
		free(rescuer_types[i]->twins);						    // libero l'array di puntatori ai gemelli digitali		
		free(rescuer_types[i]);								    // libero il puntatore al rescuer_type_t 
	}	
	free(rescuer_types);								        // libero l'array di puntatori ai rescuer_types
}


void free_emergencies(emergencies_t *emergencies){
	free_emergency_types(emergencies->types);
	emergencies->types = NULL;
	free(emergencies);
}

void free_rescuer_requests(rescuer_request_t **rescuer_requests){
	for(int i = 0; rescuer_requests[i] != NULL; i++)
		free(rescuer_requests[i]);	                            // libero il puntatore al rescuer_request_t
	free(rescuer_requests);
}

void free_emergency_types(emergency_type_t **emergency_types){
	for(int i = 0; emergency_types[i] != NULL; i++){
		free(emergency_types[i]->emergency_desc);				// libero il puntatore al nome 
		free_rescuer_requests(emergency_types[i]->rescuers); 	// libero ogni rescuer_request_t
		free(emergency_types[i]);								// libero il puntatore alla singola istanza emergency_type_t 
	}
	free(emergency_types);	                                    // libero l'array di puntatori agli emergency_types
}


