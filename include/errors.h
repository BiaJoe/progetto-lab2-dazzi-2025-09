#ifndef ERRORS_H
#define ERRORS_H


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <mqueue.h>
#include <semaphore.h>
#include <threads.h>
#include <string.h>

#define MQ_FAILED ((mqd_t)-1)

#define DEATH_BY_ERROR(msg, error_name) \
	do { \
		fprintf(stderr, "\n[%s at %s: %d] %s\n", error_name, __FILE__, __LINE__, msg); \
		exit(EXIT_FAILURE); \
	} while(0)
 
// controllo di errore comune
#define ERR_CHECK(cond, msg) do { if (cond) DEATH_BY_ERROR(msg, "ERROR"); } while (0)
// macro wrap delle chiamate di sistema
#define SYSV(call, illegal_value, error_message) do { if ((call) == illegal_value) DEATH_BY_ERROR(error_message, "ERROR (SYS)"); } while (0)
// macro wrap delle chiamate di sistema che come errore ritornano -1
#define SYSC(call, error_message) SYSV((call), -1, error_message)


#define check_error_fopen(fp) 						ERR_CHECK((fp) == NULL, "fopen")
#define check_error_memory_allocation(p) 			ERR_CHECK(!(p), "memory allocation")

#define check_error_mq_open(mq) 					ERR_CHECK((mq) == (mqd_t)-1, "mq_open") 
#define check_error_mq_send(i) 						ERR_CHECK((i) == -1, "mq_send")
#define check_error_mq_receive(b) 					ERR_CHECK((b) == -1, "mq_receive")
#define check_error_mtx_init(call)  				ERR_CHECK((call) != thrd_success, "mutex init")
#define check_error_cnd_init(call)  				ERR_CHECK((call) != thrd_success, "cnd init")
#define check_error_thread_create(call) 			ERR_CHECK((call) != thrd_success, "thread create")

#define check_error_shm_open(fd)        			ERR_CHECK((fd) == -1, "shm_open")
#define check_error_shm_unlink(rc)     				ERR_CHECK((rc) == -1, "shm_unlink")
#define check_error_shm_mmap(ptr)       			ERR_CHECK((ptr) == MAP_FAILED, "mmap")
#define check_error_shm_munmap(rc)      			ERR_CHECK((rc) == -1, "munmap")
#define check_error_shm_close(rc)       			ERR_CHECK((rc) == -1, "close")
#define check_error_ftruncate(rc)       			ERR_CHECK((rc) == -1, "ftruncate")

#define check_error_sem_open(sem) 					ERR_CHECK((sem) == SEM_FAILED, "sem_open")
#define check_error_sem_wait(sem) 					ERR_CHECK((sem) == -1, "sem_wait")
#define check_error_sem_post(sem) 					ERR_CHECK((sem) == -1, "sem_post")
#define check_error_sem_close(sem) 					ERR_CHECK((sem) == -1, "sem_close")
#define check_error_sem_unlink(sem) 				ERR_CHECK((sem) == -1, "sem_unlink")

#define check_error_fork(pid) 						ERR_CHECK((pid) < 0, "fork_failed")

#endif