#ifndef BRESENHAM_H
#define BRESENHAM_H

#include "utils.h"

bresenham_trajectory_t *mallocate_bresenham_trajectory();
int compute_bresenham_step(int x, int y, bresenham_trajectory_t *trajectory, int cells_per_step, int *x_step, int *y_step);
void change_bresenham_trajectory(bresenham_trajectory_t *t, int current_x, int current_y, int new_x, int new_y);

#endif

