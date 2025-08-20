#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

//error checking macros (le uniche macro del progetto che non sono in all caps)
 
#define check_error(cond, msg)                                      \
  do {                                                              \
    if (cond) {                                                     \
      fprintf(stderr, "\n[ERROR at %s: %d] ", __FILE__, __LINE__);  \
			perror(msg);                                            \
      exit(EXIT_FAILURE);                                           \
    }                                                               \
  } while (0)
	
#define check_error_fopen(fp) 						check_error((fp) == NULL, "fopen")
#define check_error_memory_allocation(p) 			check_error(!(p), "memory allocation")

#define check_error_mq_open(mq) 					check_error((mq) == (mqd_t)-1, "mq_open") 
#define check_error_mq_send(i) 						check_error((i) == -1, "mq_send")
#define check_error_mq_receive(b) 					check_error((b) == -1, "mq_receive")
#define try_to_open_queue(mq, queue_name, max_attempts, nanoseconds_interval) 		\
	do {																																\
		if ((mq) == (mqd_t)-1) { 																					\
			int attempts = 0; 																							\
			while (attempts < (max_attempts)) { 														\
				mq = mq_open(queue_name, O_WRONLY); 													\
				if (mq != (mqd_t)-1) 																					\
					break; 																											\
				struct timespec req = {																				\
						.tv_sec = 0,																							\
						.tv_nsec = (nanoseconds_interval)													\
				};																														\
				nanosleep(&req, NULL);																				\
				attempts++; 																									\
			} 																															\
			if (mq == (mqd_t)-1) {																					\
				perror("mq_open fallita dopo 10 tentativi"); 									\
				exit(EXIT_FAILURE); 																					\
			} 																															\
		}																																	\
	} while (0);



#define check_error_fork(pid) 						check_error((pid) < 0, "fork_failed")
#define check_error_syscall(call, m)				check_error((call) == -1, m)
#define check_error_mtx_init(call)  				check_error((call) != thrd_success, "mutex init")
#define check_error_cnd_init(call)  				check_error((call) != thrd_success, "cnd init")
#define check_error_thread_create(call) 			check_error((call) != thrd_success, "thread create")


#endif