// log_test.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "log.h"

#define RESCUERS_CONF        "conf/rescuers.conf"
#define EMERGENCY_TYPES_CONF "conf/emergency_types.conf"
#define ENV_CONF             "conf/env.conf"

static const logging_config_t server_logging_config = {
    .log_file                       = "log.txt",
    .log_file_ptr                   = NULL,
    .logging_syntax                 = "%-16s %-6s %-35s %s\n",
    .non_applicable_log_id_string   = "N/A",
    .log_to_file                    = true,
    .log_to_stdout                  = true,
    .flush_every_n                  = 64
};


// piccola pausa cross‑platform con C11 threads.h
static void tiny_sleep_ms(long ms) {
    struct timespec t = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    thrd_sleep(&t, NULL);
}

int main(void) {
    // 1) Avvio logging come SERVER (apre la coda, il thread logger, ecc.)
    if (log_init(LOG_ROLE_SERVER, server_logging_config) != 0) {
        fprintf(stderr, "log_init fallita\n");
        return 1;
    }

    // 2) Alcuni eventi di prova con ID automatico (contatori indipendenti per tipo evento)
    log_event(AUTOMATIC_LOG_ID, DEBUG, "Debug (auto id) #1");
    log_event(AUTOMATIC_LOG_ID, DEBUG, "Debug (auto id) #2");
    log_event(AUTOMATIC_LOG_ID, PARSING_STARTED, "Parsing iniziato di %s", RESCUERS_CONF);
    log_event(AUTOMATIC_LOG_ID, EMERGENCY_REQUEST_RECEIVED, "Richiesta emergenza fittizia arrivata");

    // 3) Eventi con ID esplicito e con ID NON_APPLICABILE
    log_event(42, SERVER, "Messaggio SERVER con id esplicito = 42");
    log_event(NON_APPLICABLE_LOG_ID, CLIENT, "Messaggio CLIENT con id N/A");

    // 4) Messaggio lungo per verificare il troncamento a MAX_LOG_EVENT_FORMATTED_MESSAGE_LENGTH
    char long_msg[1024];
    memset(long_msg, 'X', sizeof(long_msg));
    long_msg[sizeof(long_msg) - 1] = '\0';
    log_event(AUTOMATIC_LOG_ID, DEBUG, "Lungo: %s", long_msg);

    // 5) Piccola attesa così il thread logger consuma qualcosa prima della chiusura
    tiny_sleep_ms(20);

    // 6) Chiusura ordinata: invia LOGGING_ENDED e join del thread
    log_close();

    return 0;
}