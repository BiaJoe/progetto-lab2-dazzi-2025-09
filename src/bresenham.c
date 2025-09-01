#include "bresenham.h"
#include "log.h"

bresenham_trajectory_t *mallocate_bresenham_trajectory(){
	bresenham_trajectory_t *b = (bresenham_trajectory_t *)malloc(sizeof(bresenham_trajectory_t));
	check_error_memory_allocation(b);
	return b;
}

// funzione riciclabile per ogni entità che segue la linea di bresenham. 
// calcola un passo lungo la linea che percorre al massimo cells_per_step celle in totale
// cammina una cella alla volta finchè o è arrivata o ha camminato esattamente cells_per_step celle
// ritorna se siamo arrivati a destinazione o no
bool compute_bresenham_step(int x, int y, bresenham_trajectory_t *trajectory, int cells_per_step, int *x_step, int *y_step){
	if(!trajectory) return false;										// senza traiettoria non si fa nulla
	*x_step = 0;
	*y_step = 0;
	if(cells_per_step < 0) return true; 		
	
	*x_step = 0;
	*y_step = 0;

	int distance_x = ABS(x - trajectory->x_target);
	int distance_y = ABS(y - trajectory->y_target);
	int manhattan_distance = distance_x + distance_y;

	if (manhattan_distance <= cells_per_step){		// caso in cui si arriva in meno di un passo
    *x_step = ABS(x - trajectory->x_target) * trajectory->sx;
    *y_step = ABS(y - trajectory->y_target) * trajectory->sy;
    return true;
	}	

	int xA = x;
	int yA = y;
	int xB = trajectory->x_target;
	int yB = trajectory->y_target;
	int dx = trajectory->dx;
	int dy = trajectory->dy;
	int sx = trajectory->sx;
	int sy = trajectory->sy;
	int i = 0;

	while (i < cells_per_step) {			// faccio un passo alla volta percorrendo la linea di Bresenham 
		if (xA == xB && yA == yB) 			// siamo arrivati
			return true;				
		int e2 = 2 * trajectory->err;		// l'errore serve a dirci se siamo più lontani sulla x o sulla y 
		if (e2 >= -dy) {								// se siamo più lontani sulla x facciamo un passo sulla x
			trajectory->err -= dy;				// aggiorno l'errore 
			xA += sx;											// faccio un passo sull'asse x
			(*x_step) += sx;	
			i++;													// aggiorno il numero di passi fatti sull'asse x
		}
		if(i >= cells_per_step)					// potremmo aver raggiunto adesso i == cells_per_step
			break;
		if (e2 <= dx) {									// se invece siamo più lontani sulla y si fa la stessa cosa ma sulla y
			trajectory->err += dx;
			yA += sy;		
			(*y_step) += sy;	
			i++;				
		}
	}

	return (xA == xB && yA == yB) ? true : false;								// siamo arrivati?
}

void change_bresenham_trajectory(bresenham_trajectory_t *t, int current_x, int current_y, int new_x, int new_y){
	if(!t) return;
	t->x_target = new_x;							// le coordinate da raggiungere cambiano
	t->y_target = new_y;							// con loro si aggiorna il resto della traiettoria
	t->dx = ABS(new_x - current_x);				
	t->dy = ABS(new_y - current_y);
	t->sx = (current_x < new_x) ? 1 : -1;
	t->sy = (current_y < new_y) ? 1 : -1;
	t->err = t->dx - t->dy;
}