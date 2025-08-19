#include "utils.h"

// dice se la linea è vuota o no
// non altera il pointer perchè viene passato per valore
// quindi è sicura da usare anche per linee che dopo servono intatte
int is_line_empty(char *line){
	if (line == NULL) return 1;
	while(*line) if(!isspace(*(line++))) return 0;
	return 1;
}

// funzione scitta da me perchè conveniente
// prende una stringa e se essa rappresenta un numero, lo ritorna 
// se non lo rappresenta allora errno = EINVAL;
// gestisce:
// - spazi iniziali
// - segni '-' e '+'
// - valori non accettati (EINVAL)
// non gestisce l'overflow
int my_atoi(char a[]){
	char c;
	int i = 0, sign = 1, res = 0;

	// se la stringa è vuota la rifiuta
	if(a == NULL){
		errno = EINVAL;
		return 0;
	}

	// salta tutti gli spazi vuoti
	for (i = 0; isspace(a[i]); i++) ;		

	// salva il segno del numero
	sign = (a[i] == '-') ? -1 : 1;			
	if(a[i] == '-' || a[i] == '+') 
		i++;

	// controllo che ci sia almeno una cifra
	if(a[i] == '\0'){										
		errno = EINVAL;
		return 0;
	}

	// se trovo una non-cifra scarto tutto
	while ((c = a[i]) != '\0') {
		if (c < '0' || c > '9') {					
			errno = EINVAL;
			return 0;
		}
		res = 10 * res + (c - '0');
		i++;
	}

	// mette il segno e ritona	
	return sign * res;									
}

// funzioni per accedere a strutture

rescuer_type_t * get_rescuer_type_by_name(char *name, rescuer_type_t **rescuer_types){
	for(int i = 0; rescuer_types[i] != NULL; i++){
		if(strcmp(rescuer_types[i]->rescuer_type_name, name) == 0){
			return rescuer_types[i];
		}
	}
	return NULL;
}

char* get_name_of_rescuer_requested(rescuer_request_t *rescuer_request){
	rescuer_type_t *rescuer_type = (rescuer_type_t *)rescuer_request->type;
	return rescuer_type->rescuer_type_name;
}

emergency_type_t * get_emergency_type_by_name(char *name, emergency_type_t **emergency_types){
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

rescuer_type_t *get_rescuer_type_by_index(server_context_t *ctx, int i){
	return ctx->rescuer_types[i % ctx->rescuer_types_count];
}
 
rescuer_digital_twin_t *get_rescuer_digital_twin_by_index(rescuer_type_t *r, int rescuer_digital_twin_index){
	return (r == NULL) ? NULL : r->twins[rescuer_digital_twin_index];
}

int get_time_before_emergency_timeout_from_priority(int p){
	switch (p) {
		case MEDIUM_EMERGENCY_PRIORITY: return MAX_TIME_IN_MEDIUM_PRIORITY_BEFORE_TIMEOUT; break;
		case MAX_EMERGENCY_PRIORITY: 		return MAX_TIME_IN_MAX_PRIORITY_BEFORE_TIMEOUT; break;
		default:												return INVALID_TIME;		
	}
}


// funzioni per liberare strutture importanti

void free_rescuer_types(rescuer_type_t **rescuer_types){
	for(int i = 0; rescuer_types[i] != NULL; i++){
		free(rescuer_types[i]->rescuer_type_name);					//libero il puntatore al nome 
		for(int j = 0; j < rescuer_types[i]->amount; j++){	// libero ogni gemello digitale
			free(rescuer_types[i]->twins[j]->trajectory);			// libero la struttura usata per gestire i suoi movimenti
			free(rescuer_types[i]->twins[j]);
		}
		free(rescuer_types[i]->twins);											// libero l'array di puntatori ai gemelli digitali		
		free(rescuer_types[i]);															// libero il puntatore al rescuer_type_t 
	}	
	free(rescuer_types);																	// libero l'array di puntatori ai rescuer_types
}

void free_rescuer_requests(rescuer_request_t **rescuer_requests){
	for(int i = 0; rescuer_requests[i] != NULL; i++)
		free(rescuer_requests[i]);	// libero il puntatore al rescuer_request_t
	free(rescuer_requests);
}

void free_emergency_types(emergency_type_t **emergency_types){
	for(int i = 0; emergency_types[i] != NULL; i++){
		free(emergency_types[i]->emergency_desc);							// libero il puntatore al nome 
		free_rescuer_requests(emergency_types[i]->rescuers); 	// libero ogni rescuer_request_t
		free(emergency_types[i]);															// libero il puntatore alla singola istanza emergency_type_t 
	}
	free(emergency_types);	// libero l'array di puntatori agli emergency_types
}


