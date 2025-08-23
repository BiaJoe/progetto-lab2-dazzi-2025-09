#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "errors.h"
#include "structs.h"

// macro per rendere il codice più breve e leggibile

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MANHATTAN(x1,y1,x2,y2) (ABS((x1) - (x2)) + ABS((y1) - (y2)))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define TRUNCATE_STRING_AT_MAX_LENGTH(s,maxlen) \
	do { \
		if(strlen(s) >= (maxlen)) \
			(s)[(maxlen)-1] = '\0'; \
	} while(0)

#define LOCK(m) mtx_lock(&(m))
#define UNLOCK(m) mtx_unlock(&(m))

// funzioni di utilità generale
int is_line_empty(char *line);
int my_atoi(char a[]);



#endif