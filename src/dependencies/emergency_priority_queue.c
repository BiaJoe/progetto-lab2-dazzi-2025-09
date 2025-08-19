#include "emergency_priority_queue.h"

// funzioni di gestione della coda di emergenze

emergency_t *mallocate_emergency(server_context_t *ctx, char* name, int x, int y, time_t timestamp){
	emergency_t *e = (emergency_t *)malloc(sizeof(emergency_t));
	check_error_memory_allocation(e);
	emergency_type_t *type = get_emergency_type_by_name(name, ctx->emergency_types);
	emergency_status_t status = WAITING;
	int rescuer_count = 0;
	for(int i = 0; i < type->rescuers_req_number; i++)		// calcola il numero totale di rescuer che servono all'emergenza
		rescuer_count += type->rescuers[i]->required_count;
	rescuer_digital_twin_t **rescuer_twins = (rescuer_digital_twin_t **)calloc(rescuer_count + 1, sizeof(rescuer_digital_twin_t *));
	check_error_memory_allocation(rescuer_twins);			// alloca il numero necessario di rescuers (per ora tutti a NULL)

	e->id = ctx->valid_emergency_request_count;
	e->priority = (int) type->priority; 					// prendo la priorità dall'emergency_type, potrei doverla modificare in futuri
	e->type = type;
	e->status = status;
	e->x = x;
	e->y = y;
	e->time = timestamp;
	e->rescuer_count = rescuer_count;
	e->rescuer_twins = rescuer_twins;
	e->time_since_started_waiting = INVALID_TIME;
	e->time_since_it_was_assigned = INVALID_TIME;
	e->time_spent_existing = 0;

	return e;
}

void free_emergency(emergency_t* e){
	if(!e) return;
	free(e->rescuer_twins);			// libero il puntatore ai gemelli digitali dei rescuers (non i gemelli stessi ovviamente)
	free(e);
}


emergency_node_t* mallocate_emergency_node(emergency_t *e){
	emergency_node_t* n = (emergency_node_t*)malloc(sizeof(emergency_node_t));
	check_error_memory_allocation(n);
	n -> list = NULL;
	n -> rescuers_found = NO;
	n -> rescuers_are_arriving = NO;
	n -> rescuers_have_arrived = NO;
	n -> time_estimated_for_rescuers_to_arrive = INVALID_TIME; // inizialmente non so quanto ci metteranno i rescuers ad arrivare
	n -> emergency = e;
	n -> prev = NULL;
	n -> next = NULL;
	check_error_mtx_init(mtx_init(&(n->mutex), mtx_plain));
	return n;
}

void free_emergency_node(emergency_node_t* n){
	if(!n) return;
	mtx_destroy(&(n->mutex));		// distruggo il mutex del nodo
	free_emergency(n->emergency);
	free(n);
}

emergency_list_t *mallocate_emergency_list(){
	emergency_list_t *el = (emergency_list_t *)malloc(sizeof(emergency_list_t));
	check_error_memory_allocation(el);	
	el -> head = NULL;
	el -> tail = NULL;
	el -> node_amount = 0;
	check_error_mtx_init(mtx_init(&(el->mutex), mtx_plain));
	return el;
}

void free_emergency_list(emergency_list_t *list){
	if(!list) return;
		emergency_node_t *current = list->head;
		while (current != NULL) {					// percorro la lista da head a tail->next (=NULL) e libero un nodo alla volta
			emergency_node_t *next = current->next; // l'ultima volta next è null
			free_emergency_node(current);
			current = next;
		}
	mtx_destroy(&(list->mutex));
	free(list);
}

emergency_queue_t *mallocate_emergency_queue(){
	emergency_queue_t *q = (emergency_queue_t*)malloc(sizeof(emergency_queue_t));
	for(int i = MIN_EMERGENCY_PRIORITY; i <= MAX_EMERGENCY_PRIORITY; i++)
		q->lists[i] = mallocate_emergency_list();
	q->is_empty = YES; 																
	check_error_cnd_init(cnd_init(&(q->not_empty)));	
	check_error_mtx_init(mtx_init(&(q->mutex), mtx_plain));
	return q;
}

int is_queue_empty(emergency_queue_t *q){
	return q->is_empty;
}

void free_emergency_queue(emergency_queue_t *q){
	for(int i = MIN_EMERGENCY_PRIORITY; i <= MAX_EMERGENCY_PRIORITY; i++)
		free_emergency_list(q->lists[i]);
	cnd_destroy(&(q->not_empty));		// distruggo la condizione di notifica
	mtx_destroy(&(q->mutex));
	free(q);
}

// inserisce un nodo in fondo alla lista
void append_emergency_node(emergency_list_t* list, emergency_node_t* node){
	if(!list || !node){					// se ho allocato un nodo da appendere in una lista insesistente
		return;							// per cui devo liberarlo e dimenticarmene
	}
	if (list->node_amount == 0){		// se la lista è vuota 
		list -> head = node;			// il nodo diventa sia capo che coda
		node -> prev = NULL;
	} else {
		list -> tail -> next = node; 	// il tail diventa il penultimo nodo che punta all'ultimo con next		
		node -> prev = list->tail;		// l'ultimo nodo punta il penultimo con prev
	}			
	node -> next = NULL; 				// il nodo non ha nodi dopo di lui
	node -> list = list;				// la lista di appartenenza del nodo diventa list
	list -> tail = node;				// l'ultimo nodo (tail) diventa ufficialmente node
	list -> node_amount += 1;			// il numero di nodi aumenta di 1 !!!
}

void push_emergency_node(emergency_list_t* list, emergency_node_t *node){
	if(!list || !node){					// se ho allocato un nodo da appendere in una lista insesistente
		return;							// per cui devo liberarlo e dimenticarmene
	}
	if (list->node_amount == 0){		// se la lista è vuota il nodo diventa sia capo che coda				
		list -> tail = node;
		node -> next = NULL;
	} else {
		list -> head -> prev = node;	// il nodo diventa il nuovo primo nodo
		node -> next = list->head;		// il nuovo nodo punta al vecchio primo nodo
	}			
	list -> head = node;				// il nuovo nodo diventa la nuova testa della lista
	node -> list = list;				// la lista di appartenenza del nodo diventa list
	node -> prev = NULL;				// il nuovo nodo non ha un precedente perchè è la testa
	list -> node_amount += 1;			// il numero di nodi aumenta di 1 !!!
}

int is_the_first_node_of_the_list(emergency_node_t* node){
	return (node->prev == NULL) ? YES : NO ;
}

int is_the_last_node_of_the_list(emergency_node_t* node){
	return (node->next == NULL) ? YES : NO ;
}

void remove_emergency_node_from_its_list(emergency_node_t* node){
	if(!node || !(node->list)) return;								// se il nodo non è in nessuna lista non c'è niente da fare
		if (is_the_first_node_of_the_list(node)) {					// trattamento del primo nodo:
			node->list->head = node->next;							// - aggiorno la testa della lista
			if (node->list->head) node->list->head->prev = NULL;	// - la testa ha NULL come precedente
		} else node->prev->next = node->next; 						// tutti gli altri collegano il precedente al successivo
		if (is_the_last_node_of_the_list(node)) {					// trattamento della coda del nodo:
			node->list->tail = node->prev;							// - aggiorno la coda della lista
			if (node->list->tail) node->list->tail->next = NULL;	// - il next della coda è sempre NULL
		} else node->next->prev = node->prev;						// tutti i nodi tranne l'ultimo
	node->prev = NULL;												// annullo i puntatori del nodo 
	node->next = NULL;												// per non rischiare di accedervi quando è fuori
	node->list->node_amount -= 1;									// decremento il numero di nodi nella lista
}

void change_emergency_node_list_append(emergency_node_t *n, emergency_list_t *new_list){
	remove_emergency_node_from_its_list(n);
	append_emergency_node(new_list, n);
}

void change_emergency_node_list_push(emergency_node_t *n, emergency_list_t *new_list){
	remove_emergency_node_from_its_list(n);
	push_emergency_node(new_list, n);
}

emergency_node_t* decapitate_emergency_list(emergency_list_t* list){
	if(!list || list->node_amount == 0) return NULL;				// controllo la vuotezza
	emergency_node_t* head = list->head;				
	remove_emergency_node_from_its_list(head);				
	head -> prev = NULL;											// il nodo estratto non ha un prev
	head -> next = NULL;											// nè un next
	return head;
}

void enqueue_emergency_node(emergency_queue_t* q, emergency_node_t *n){
	if(!q || !n) return; 											// se la coda o il nodo sono null non faccio nulla
	if(q->is_empty){		
		q->is_empty = NO;
		cnd_signal(&q->not_empty); 									// segnalo che la coda non è più vuota, un worker thread può processare l'emergenza
	}	
	int p = n->emergency->priority; 					
	append_emergency_node(q->lists[p], n);							// appendo il nodo alla lista
}

emergency_node_t* dequeue_emergency_node(emergency_queue_t* q){
	if(!q) return NULL;
	int p = MAX_EMERGENCY_PRIORITY;
	emergency_node_t *h = NULL; 									// head
	while(p >= MIN_EMERGENCY_PRIORITY){								// tento la decapitate ad ogni priorità dalla più grande alla più piccola
		h = decapitate_emergency_list(q->lists[p--]);					
		if (h != NULL) break;										// se ho trovato un h valido vuol dire che ho decapitato quella lista con successo
	}
	if(h == NULL) q->is_empty = YES;								// se non ho trovato nodi la lista deve essere vuota
	return h; 														// ritorno il nodo decapitato	
}

void change_node_priority_list(emergency_queue_t* q, emergency_node_t* n, int newp){
	if (newp < MIN_EMERGENCY_PRIORITY || newp > MAX_EMERGENCY_PRIORITY)
		return;
	remove_emergency_node_from_its_list(n);
	n->emergency->priority = newp; 																		
	append_emergency_node(q->lists[newp], n);
}





