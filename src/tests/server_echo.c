// src/tests/server_echo.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"       // usa threads.h internamente e avvia il logger thread
#include "errors.h"    // per i check_error_* se vuoi usarli altrove

// Se hai un header condiviso, includilo. Altrimenti mantieni questi default coerenti col client.
#ifndef SHM_NAME
#define SHM_NAME "/emergency_shm"
#endif
#ifndef SEM_NAME
#define SEM_NAME "/emergency_shm_ready"
#endif
#ifndef MAX_EMERGENCY_QUEUE_NAME_LENGTH
#define MAX_EMERGENCY_QUEUE_NAME_LENGTH 128
#endif
#ifndef MAX_EMERGENCY_REQUEST_LENGTH
#define MAX_EMERGENCY_REQUEST_LENGTH 512
#endif

static int is_stop_message(const char* s) {
#ifdef STOP_MESSAGE_FROM_CLIENT
    return strcmp(s, STOP_MESSAGE_FROM_CLIENT) == 0;
#else
    return strcmp(s, "STOP") == 0;
#endif
}

int main(void) {
    // 0) Logging: il server crea /log_queue e lancia il logger thread definito in log.c
    if (log_init(LOG_ROLE_SERVER) != 0) {
        fprintf(stderr, "log_init(LOG_ROLE_SERVER) failed\n");
        return 1;
    }
    log_event(AUTOMATIC_LOG_ID, SERVER, "server_echo: avvio");

    // 1) Crea la coda delle emergenze (lettura) — decidi il nome e mettilo in SHM
    const char *EMERGENCY_QUEUE_NAME = "/test_emergencies";
    struct mq_attr qattr = {
        .mq_flags = 0,
        .mq_maxmsg = 32,
        .mq_msgsize = MAX_EMERGENCY_REQUEST_LENGTH + 1,
        .mq_curmsgs = 0
    };
    mqd_t mq = mq_open(EMERGENCY_QUEUE_NAME, O_CREAT | O_RDONLY, 0600, &qattr);
    if (mq == (mqd_t)-1) {
        perror("mq_open emergency RD");
        log_fatal_error("server_echo: mq_open emergenze fallita (%s)", EMERGENCY_QUEUE_NAME);
    }
    log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_SERVER, "coda emergenze pronta: %s", EMERGENCY_QUEUE_NAME);

    // 2) Pubblica il nome coda via SHM e sblocca i client con il semaforo
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd == -1) log_fatal_error("shm_open fallita: %s", strerror(errno));
    if (ftruncate(fd, sizeof(client_server_shm_t)) == -1)
        log_fatal_error("ftruncate shm fallita: %s", strerror(errno));

    client_server_shm_t *shm = mmap(NULL, sizeof(*shm), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) log_fatal_error("mmap shm fallita: %s", strerror(errno));
    close(fd);

    memset(shm->queue_name, 0, sizeof(shm->queue_name));
    snprintf(shm->queue_name, sizeof(shm->queue_name), "%s", EMERGENCY_QUEUE_NAME);

    sem_unlink(SEM_NAME); // pulizia preventiva
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0600, 0);
    if (sem == SEM_FAILED) log_fatal_error("sem_open fallita: %s", strerror(errno));
    if (sem_post(sem) == -1) log_fatal_error("sem_post fallita: %s", strerror(errno));
    sem_close(sem);
    log_event(AUTOMATIC_LOG_ID, SERVER, "shm+sem pronti, client sbloccati");

    // 3) Loop: ricevi emergenze, stampa, logga; esci su STOP
    char msg[MAX_EMERGENCY_REQUEST_LENGTH + 1];
    for (;;) {
        ssize_t n = mq_receive(mq, msg, sizeof(msg), NULL);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("mq_receive");
            log_event(AUTOMATIC_LOG_ID, SERVER, "mq_receive errore: %s", strerror(errno));
            break;
        }
        msg[n] = '\0';

        // stampa “business”
        printf("EMERGENZA: %s\n", msg);
        fflush(stdout);

        // logging strutturato
        log_event(AUTOMATIC_LOG_ID, EMERGENCY_REQUEST_RECEIVED, "ricevuta emergenza: %s", msg);

        if (is_stop_message(msg)) {
            log_event(AUTOMATIC_LOG_ID, SERVER, "server_echo: STOP ricevuto, chiusura");
            break;
        }
    }

    // 4) Cleanup
    mq_close(mq);
    mq_unlink(EMERGENCY_QUEUE_NAME);

    munmap(shm, sizeof(*shm));
    shm_unlink(SHM_NAME);

    sem_unlink(SEM_NAME);

    log_close(); // chiude logger thread e /log_queue
    return 0;
}