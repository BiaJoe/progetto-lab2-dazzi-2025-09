#include "structs.h"

// ======================================= //
//    CONFIGURAZIONE GESTIONE EMERGENZE    //
// ======================================= //

#define TIME_BEFORE_AN_EMERGENCY_SHOULD_BE_CANCELLED_TICKS 300

#define NO_TIMEOUT   -1
#define NO_PROMOTION -1


// struttura che permette di decidere quali sono le priorità che le emergenze possono avere
// non importa ordine, consecutività o altro, basta che siano uniche
static const priority_rule_t priority_lookup_table[] = {
//   priority number        secondi prima della promozione,     tempo prima di andare in timeout
    {0,                     120,                                NO_TIMEOUT},
    {1,                     NO_PROMOTION,                       30},
    {2,                     NO_PROMOTION,                       10}
};

static const int priority_count = (int)(sizeof(priority_lookup_table)/sizeof(priority_lookup_table[0]));

#define MIN_TIME_TO_MANAGE 1
#define MAX_TIME_TO_MANAGE 1000


// ======================================= //
//  CONFIGURAZIONE DEL SISTEMA DI PARSING  //
// ======================================= //

#define RESCUERS_CONF        "conf/rescuers.conf"
#define EMERGENCY_TYPES_CONF "conf/emergency_types.conf"
#define ENV_CONF             "conf/env.conf"

#define MIN_X_COORDINATE_ABSOLUTE_VALUE 0
#define MAX_X_COORDINATE_ABSOLUTE_VALUE 1024
#define MIN_Y_COORDINATE_ABSOLUTE_VALUE 0
#define MAX_Y_COORDINATE_ABSOLUTE_VALUE 1024

#define MIN_RESCUER_SPEED 1
#define MAX_RESCUER_SPEED 100
#define MIN_RESCUER_AMOUNT 1
#define MAX_RESCUER_AMOUNT 1000
#define MIN_RESCUER_REQUIRED_COUNT 1
#define MAX_RESCUER_REQUIRED_COUNT 32

#define MAX_RESCUER_NAME_LENGTH 128
#define MAX_EMERGENCY_NAME_LENGTH 64
#define MAX_RESCUER_REQUESTS_LENGTH 1024
#define MAX_RESCUER_TYPES_COUNT 64
#define MAX_EMERGENCY_TYPES_COUNT 128
#define MAX_RESCUER_REQ_NUMBER_PER_EMERGENCY 16
#define MAX_ENV_FIELD_LENGTH 32

#define STR_HELPER(x) #x    // todo: cleanup utils.h, metterlo lì e includerlo qui
#define STR(x) STR_HELPER(x)

#define RESCUERS_SYNTAX "[%" STR(MAX_RESCUER_NAME_LENGTH) "[^]]][%d][%d][%d;%d]"
#define RESCUER_REQUEST_SYNTAX "%" STR(MAX_RESCUER_NAME_LENGTH) "[^:]:%d,%d"
#define EMERGENCY_TYPE_SYNTAX "[%" STR(MAX_EMERGENCY_NAME_LENGTH) "[^]]] [%hd] %" STR(MAX_RESCUER_REQUESTS_LENGTH) "[^\n]"
#define EMERGENCY_REQUEST_SYNTAX "%" STR(MAX_EMERGENCY_NAME_LENGTH) "[^ ] %d %d %ld"
#define EMERGENCY_REQUEST_ARGUMENT_SEPARATOR " " // nel file si separano gli argomenti con lo spazio, questo impone che i nomi non contengano spazi tra l'altro
#define RESCUER_REQUESTS_SEPARATOR ";"

#define ENV_CONF_QUEUE_SYNTAX 	"queue= %" STR(EMERGENCY_QUEUE_NAME_LENGTH) "s"
#define ENV_CONF_HEIGHT_SYNTAX 	"height= %d"
#define ENV_CONF_WIDTH_SYNTAX 	"width= %d"

// ======================================= //
//  CONFIGURAZIONE DEL SISTEMA DI LOGGING  //
// ======================================= //

#define LOG_FILE "log.txt"
#define NON_APPLICABLE_LOG_ID_STRING "N/A"
#define LOG_EVENT_STRING_SYNTAX "%-16s %-6s %-35s %s\n"
// #define LOG_EVENT_STRING_SYNTAX "[%s] [%s] [%s] [%s]\n"

#define LOG_TO_FILE             1         // 1 = si logga in LOG_FILE
#define LOG_TO_STDOUT           1         // 1 = si logga su stdout
#define LOG_RING_CAP            4096
#define LOG_WRITER_BLOCKING     1
#define LOG_FLUSH_EVERY_N       64


