#ifndef CONSTANTS_H
#define CONSTANTS_H

#define RESCUERS_CONF        "conf/rescuers.conf"
#define EMERGENCY_TYPES_CONF "conf/emergency_types.conf"
#define ENV_CONF             "conf/env.conf"

#define YES 1
#define NO 	0

#define MAX_FILE_LINES 	1024
#define MAX_LINE_LENGTH 1024
#define MAX_RESCUER_CONF_LINES 128
#define MAX_RESCUER_CONF_LINE_LENGTH 512
#define MAX_EMERGENCY_CONF_LINES 256
#define MAX_EMERGENCY_CONF_LINE_LENGTH 512
#define MAX_ENV_CONF_LINES 12
#define MAX_ENV_CONF_LINE_LENGTH 128

#define MAX_RESCUER_NAME_LENGTH 128
#define EMERGENCY_NAME_LENGTH 64
#define MAX_RESCUER_REQUESTS_LENGTH 1024
#define MAX_RESCUER_TYPES_COUNT 64
#define MAX_EMERGENCY_TYPES_COUNT 128
#define MAX_RESCUER_REQ_NUMBER_PER_EMERGENCY 16
#define MAX_ENV_FIELD_LENGTH 32

// log
#define MAX_LOG_EVENT_LENGTH 1024			// lungezza dei messaggi inviati da message queue a logger
#define MAX_LOG_EVENT_ID_STRING_LENGTH 16
#define LOG_EVENT_NAME_LENGTH 64
#define LOG_EVENT_CODE_LENGTH 5
#define LOG_EVENT_MESSAGE_LENGTH 128
 
#define LOG_FILE "log.txt"
#define LOG_QUEUE_NAME "/log_queue"
#define MAX_LOG_QUEUE_MESSAGES 10
#define AUTOMATIC_LOG_ID -1
#define NON_APPLICABLE_LOG_ID -2
#define NON_APPLICABLE_LOG_ID_STRING "N/A"
#define STOP_LOGGING_MESSAGE "-stop"

// le priorità possono essere anche diverse, ma ci sono delle condizioni da rispettare
// 1. devono essere in ordine crescente
// 2. devono essere consecutive, niente salti
// il motivo sta in come è stato implementato il sistema di ordinamento delle priorità nel server
#define MIN_EMERGENCY_PRIORITY 0
#define MEDIUM_EMERGENCY_PRIORITY 1
#define MAX_EMERGENCY_PRIORITY 2
#define PRIORITY_LEVELS 3 // 0,1,2

#define MIN_RESCUER_SPEED 1
#define MAX_RESCUER_SPEED 100
#define MIN_RESCUER_AMOUNT 1
#define MAX_RESCUER_AMOUNT 1000
#define MIN_RESCUER_REQUIRED_COUNT 1
#define MAX_RESCUER_REQUIRED_COUNT 32
#define MIN_X_COORDINATE_ABSOLUTE_VALUE 0
#define MAX_X_COORDINATE_ABSOLUTE_VALUE 1024
#define MIN_Y_COORDINATE_ABSOLUTE_VALUE 0
#define MAX_Y_COORDINATE_ABSOLUTE_VALUE 1024
#define MIN_TIME_TO_MANAGE 1
#define MAX_TIME_TO_MANAGE 1000

#define EMERGENCY_QUEUE_NAME_LENGTH 16
#define EMERGENCY_QUEUE_NAME "emergenze676722" // lunghezza 16
#define EMERGENCY_QUEUE_NAME_BARRED "/emergenze676722" // lunghezza 16
#define MAX_EMERGENCY_QUEUE_MESSAGE_LENGTH 512
#define STOP_MESSAGE_FROM_CLIENT "-stop"
// #define MAX_EMERGENCY_QUEUE_CAPACITY 64

#define LONG_LENGTH 20
#define INVALID_TIME -1

// sintassi varie

#define LOG_EVENT_STRING_SYNTAX "%-15ld %-5s %-35s %s\n"
// #define LOG_EVENT_STRING_SYNTAX "[%ld] [%d] [%s] [%s]\n"
#define RESCUERS_SYNTAX "[%" STR(MAX_RESCUER_NAME_LENGTH) "[^]]][%d][%d][%d;%d]"
#define RESCUER_REQUEST_SYNTAX "%" STR(MAX_RESCUER_NAME_LENGTH) "[^:]:%d,%d"
#define EMERGENCY_TYPE_SYNTAX "[%" STR(EMERGENCY_NAME_LENGTH) "[^]]] [%hd] %" STR(MAX_RESCUER_REQUESTS_LENGTH) "[^\n]"
#define EMERGENCY_REQUEST_SYNTAX "%" STR(EMERGENCY_NAME_LENGTH) "[^ ] %d %d %ld"
#define EMERGENCY_REQUEST_ARGUMENT_SEPARATOR " " // nel file si separano gli argomenti con lo spazio, questo impone che i nomi non contengano spazi tra l'altro
#define RESCUER_REQUESTS_SEPARATOR ";"

#define ENV_CONF_VALID_LINES_COUNT 3
#define ENV_CONF_QUEUE_SYNTAX 	"queue= %" STR(EMERGENCY_QUEUE_NAME_LENGTH) "s"
#define ENV_CONF_HEIGHT_SYNTAX 	"height= %d"
#define ENV_CONF_WIDTH_SYNTAX 	"width= %d"
#define ENV_CONF_QUEUE_TURN			0
#define ENV_CONF_HEIGHT_TURN		1
#define ENV_CONF_WIDTH_TURN			2

#define MAX_TIME_IN_MIN_PRIORITY_BEFORE_PROMOTION			     120
#define MAX_TIME_IN_MEDIUM_PRIORITY_BEFORE_TIMEOUT		     30
#define MAX_TIME_IN_MAX_PRIORITY_BEFORE_TIMEOUT				     10
#define TIME_BEFORE_AN_EMERGENCY_SHOULD_BE_CANCELLED_TICKS 300



#endif