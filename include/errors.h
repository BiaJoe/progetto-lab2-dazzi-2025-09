#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

//error checking macros (le uniche macro del progetto che non sono in all caps)
 
#define check_error(cond, msg)                                      	\
do {                                                             		\
	if (cond) {                                                     	\
		fprintf(stderr, "\n[ERROR at %s: %d] ", __FILE__, __LINE__); 	\
			perror(msg);                                            	\
		exit(EXIT_FAILURE);                                           	\
	}                                                               	\
} while (0)
	
#define check_error_fopen(fp) 						check_error((fp) == NULL, "fopen")
#define check_error_memory_allocation(p) 			check_error(!(p), "memory allocation")

#define check_error_mq_open(mq) 					check_error((mq) == (mqd_t)-1, "mq_open") 
#define check_error_mq_send(i) 						check_error((i) == -1, "mq_send")
#define check_error_mq_receive(b) 					check_error((b) == -1, "mq_receive")
#define check_error_mtx_init(call)  				check_error((call) != thrd_success, "mutex init")
#define check_error_cnd_init(call)  				check_error((call) != thrd_success, "cnd init")
#define check_error_thread_create(call) 			check_error((call) != thrd_success, "thread create")

#define check_error_shm_open(fd)        			check_error((fd) == -1, "shm_open")
#define check_error_shm_unlink(rc)     				check_error((rc) == -1, "shm_unlink")
#define check_error_shm_mmap(ptr)       			check_error((ptr) == MAP_FAILED, "mmap")
#define check_error_shm_munmap(rc)      			check_error((rc) == -1, "munmap")
#define check_error_shm_close(rc)       			check_error((rc) == -1, "close")
#define check_error_ftruncate(rc)       			check_error((rc) == -1, "ftruncate")

#define check_error_sem_open(sem) 				check_error((sem) == SEM_FAILED, "sem_open")
#define check_error_sem_wait(sem) 				check_error((sem) == -1, "sem_wait")
#define check_error_sem_post(sem) 				check_error((sem) == -1, "sem_post")
#define check_error_sem_close(sem) 				check_error((sem) == -1, "sem_close")
#define check_error_sem_unlink(sem) 				check_error((sem) == -1, "sem_unlink")

#define check_error_fork(pid) 						check_error((pid) < 0, "fork_failed")

#endif