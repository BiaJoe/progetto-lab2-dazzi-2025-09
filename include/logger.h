#ifndef LOGGER_H
#define LOGGER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h> 
#include <unistd.h>


#include "utils.h"

#define LOG_BUFFER_CAPACITY 256
#define HOW_MANY_LOGS_BEFORE_FLUSHING_THE_STREAM 1

int compare_log_messages(const void *a, const void *b);
int we_should_flush(int logs_so_far);
void sort_write_flush_log_buffer(log_message_t *log_buffer, int *buffer_len, FILE *log_file);
void logger(void);

#endif