#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "log.h"

// Parametri del test
enum {
    TEST_NUM_CLIENTS    = 3,
    TEST_BURST_PER_CLIENT = 64,
    TEST_CLIENT_SLEEP_US  = 2000  // 2 ms tra alcune write per variare timing
};

static void client_work(int cid) {
    if (log_init(LOG_ROLE_CLIENT) != 0) {
        fprintf(stderr, "[client %d] log_init failed\n", cid);
        _exit(2);
    }

    // messaggi base
    log_event(AUTOMATIC_LOG_ID, CLIENT, "client %d: avvio", cid);
    log_event(42, DEBUG, "client %d: debug con id esplicito 42", cid);
    log_event(NON_APPLICABLE_LOG_ID, EMERGENCY_REQUEST_RECEIVED,
              "client %d: richiesta emergenza arrivata", cid);

    // burst per stressare la coda
    for (int i = 0; i < TEST_BURST_PER_CLIENT; ++i) {
        log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT,
                  "client %d: burst #%d", cid, i);
        if ((i % 8) == 0) usleep(TEST_CLIENT_SLEEP_US);
    }

    // Un solo client (ad es. cid == 1) prova il fatal "locale"
    if (cid == 1) {
        // usa FATAL_ERROR_CLIENT grazie alla logica in log_fatal_error
        log_fatal_error("client %d: fatal (solo client), test di uscita controllata", cid);
        // non si arriva qui
    }

    log_event(AUTOMATIC_LOG_ID, CLIENT, "client %d: fine normale", cid);
    log_close();
    _exit(0);
}

int main(void) {
    // SERVER: init e thread logger
    if (log_init(LOG_ROLE_SERVER) != 0) {
        fprintf(stderr, "[server] log_init failed\n");
        return 1;
    }

    // qualche riga dal server stesso
    log_event(AUTOMATIC_LOG_ID, SERVER, "server: logger avviato");
    log_event(NON_APPLICABLE_LOG_ID, SERVER_UPDATE, "server: update non loggato? (dipende LUT)");

    // fork dei client
    pid_t children[TEST_NUM_CLIENTS];
    for (int i = 0; i < TEST_NUM_CLIENTS; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // continua: i client già creati faranno comunque il loro lavoro
            continue;
        }
        if (pid == 0) {
            // processo figlio → client
            client_work(i);
        }
        children[i] = pid;
    }

    // Il server continua a loggare mentre i client scrivono
    for (int k = 0; k < 8; ++k) {
        log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_SERVER, "server: heartbeat #%d", k);
        usleep(3000); // 3 ms
    }

    // attende i client
    for (int i = 0; i < TEST_NUM_CLIENTS; ++i) {
        if (children[i] > 0) {
            int st = 0;
            (void)waitpid(children[i], &st, 0);
        }
    }

    // messaggi finali server
    log_event(AUTOMATIC_LOG_ID, SERVER, "server: tutti i client terminati");
    log_event(NON_APPLICABLE_LOG_ID, EMERGENCY_PARSED, "server: esempio N/A id");

    // chiusura ordinata: log_close() logga anche LOGGING_ENDED e sveglia la mq
    log_close();

    fprintf(stderr, "[server] test completato. Controlla stdout e/o %s\n", LOG_FILE);
    return 0;
}