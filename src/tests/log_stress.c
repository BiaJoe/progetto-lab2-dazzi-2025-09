#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "log.h"

/* ===========================
   PARAMETRI DELLO STRESS TEST
   =========================== */
#ifndef ST_NUM_CLIENTS
#define ST_NUM_CLIENTS              8      // tanti client concorrenti
#endif
#ifndef ST_BURST_PER_CLIENT
#define ST_BURST_PER_CLIENT         200    // burst di messaggi per client
#endif
#ifndef ST_HEARTBEATS
#define ST_HEARTBEATS               32     // quante righe manda il server durante il test
#endif
#ifndef ST_JITTER_NS_MAX
#define ST_JITTER_NS_MAX            5*1000*1000   // fino a 5 ms di jitter random
#endif
#ifndef ST_PAUSE_EVERY
#define ST_PAUSE_EVERY              25     // ogni tot messaggi, piccola pausa
#endif
#ifndef ST_PAUSE_NS
#define ST_PAUSE_NS                 500*1000  // 0.5 ms
#endif
#ifndef ST_LONG_MSG_SIZE
#define ST_LONG_MSG_SIZE            240    // messaggi quasi al limite del buffer testo
#endif

/* Utility nanosleep con controllo errori */
static inline void nsleep(long ns) {
    if (ns <= 0) return;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ns };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        /* retry sul residuo */
    }
}

/* Random tra [a,b] inclusi, seed per‑process */
static inline int rnd_between(int a, int b) {
    return a + (rand() % (b - a + 1));
}

/* Un messaggio “lungo” quasi al limite del buffer testo */
static void build_long_text(char *buf, size_t bufsz, int cid, int i) {
    size_t n = (ST_LONG_MSG_SIZE < (int)bufsz - 1) ? ST_LONG_MSG_SIZE : (bufsz - 1);
    memset(buf, 'X', n);
    buf[n] = '\0';
    // prepend info utile (senza superare bufsz)
    char head[64];
    snprintf(head, sizeof(head), "[C%d #%d] ", cid, i);
    size_t headlen = strlen(head);
    size_t shift = (headlen < n) ? headlen : n;
    memmove(buf + shift, buf, n - shift + 1);
    memcpy(buf, head, shift);
}

/* Lavoro di un client */
static void client_work(int cid) {
    // seed random per‑process
    srand((unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)(cid * 1315423911u));

    if (log_init(LOG_ROLE_CLIENT) != 0) {
        fprintf(stderr, "[client %d] log_init failed\n", cid);
        _exit(2);
    }

    log_event(AUTOMATIC_LOG_ID, CLIENT, "client %d: avvio", cid);
    log_event(42, DEBUG, "client %d: debug id=42", cid);
    log_event(NON_APPLICABLE_LOG_ID, EMERGENCY_REQUEST_RECEIVED, "client %d: richiesta emergenza", cid);

    // Primo mini-burst con jitter
    for (int i = 0; i < ST_BURST_PER_CLIENT/4; ++i) {
        log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "C%d: warmup #%d", cid, i);
        nsleep(rnd_between(0, ST_JITTER_NS_MAX));
    }

    // Burst intenso senza pause (spinge la coda)
    for (int i = 0; i < ST_BURST_PER_CLIENT/2; ++i) {
        log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "C%d: hot #%d", cid, i);
        if ((i % ST_PAUSE_EVERY) == 0) nsleep(ST_PAUSE_NS);
    }

    // Messaggi lunghi quasi a cap con jitter
    for (int i = 0; i < ST_BURST_PER_CLIENT/4; ++i) {
        char big[1024];
        build_long_text(big, sizeof(big), cid, i);
        log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_CLIENT, "%s", big);
        nsleep(rnd_between(0, ST_JITTER_NS_MAX));
    }

    // Un client prova il fatal “locale” e termina solo lui
    if (cid == 1) {
        log_fatal_error("client %d: FATAL locale di test (il server resta vivo)", cid);
        // non torna
    }

    // Chiusura normale
    log_event(AUTOMATIC_LOG_ID, CLIENT, "client %d: fine", cid);
    log_close();
    _exit(0);
}

int main(void) {
    // SERVER start
    if (log_init(LOG_ROLE_SERVER) != 0) {
        fprintf(stderr, "[server] log_init failed\n");
        return 1;
    }
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    log_event(AUTOMATIC_LOG_ID, SERVER, "server: logger avviato (stress test)");
    log_event(NON_APPLICABLE_LOG_ID, SERVER_UPDATE, "server: update (da LUT potrebbe essere filtrato)");

    // fork dei client
    pid_t children[ST_NUM_CLIENTS];
    for (int i = 0; i < ST_NUM_CLIENTS; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            client_work(i);
        }
        children[i] = pid;
        // lieve sfasamento tra client per evitare start completamente sincronizzato
        nsleep(rnd_between(0, ST_JITTER_NS_MAX));
    }

    // Heartbeats del server mentre i client spingono
    for (int k = 0; k < ST_HEARTBEATS; ++k) {
        // una parte heartbeat “normali”
        log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_SERVER, "server: heartbeat #%d", k);

        // e una parte con messaggi lunghi
        if ((k % 4) == 0) {
            char big[1024];
            build_long_text(big, sizeof(big), 999 /*server*/, k);
            log_event(AUTOMATIC_LOG_ID, MESSAGE_QUEUE_SERVER, "%s", big);
        }

        nsleep(rnd_between(0, ST_JITTER_NS_MAX));
    }

    // attende i client
    for (int i = 0; i < ST_NUM_CLIENTS; ++i) {
        if (children[i] > 0) {
            int st = 0;
            (void)waitpid(children[i], &st, 0);
        }
    }

    // messaggi finali server
    log_event(AUTOMATIC_LOG_ID, SERVER, "server: tutti i client terminati");
    log_event(NON_APPLICABLE_LOG_ID, EMERGENCY_PARSED, "server: chiusura ordinata (N/A id)");

    // chiusura logger (scrive LOGGING_ENDED e sveglia mq_receive)
    log_close();

    fprintf(stderr, "[server] STRESS TEST COMPLETATO. Controlla stdout e/o %s\n", LOG_FILE);
    return 0;
}