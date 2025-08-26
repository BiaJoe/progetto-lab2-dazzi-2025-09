#ifndef BRESENHAM_H
#define BRESENHAM_H

#include "utils.h"
#include <stdbool.h>

typedef struct {	        // struttura per la computazione del percorso di ogni rescuer
	int x_target;
	int y_target;
	int dx;					// distanze tra partenza e arrivo in x e y
	int dy;
	int sx;					// direzioni: x e y aumentano o diminuiscono?
	int sy;
	int err;				// l'errore di Bresenham: permette di decidere se muoversi sulla x o sulla y
} bresenham_trajectory_t;


bresenham_trajectory_t *mallocate_bresenham_trajectory();
bool compute_bresenham_step(int x, int y, bresenham_trajectory_t *trajectory, int cells_per_step, int *x_step, int *y_step);
void change_bresenham_trajectory(bresenham_trajectory_t *t, int current_x, int current_y, int new_x, int new_y);

#endif

