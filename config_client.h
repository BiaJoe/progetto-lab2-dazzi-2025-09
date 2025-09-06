#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include "client.h"
#include "structs.h"
#include "log.h"

static const logging_config_t client_logging_config = {
    .logging_syntax                 = "%-13s %-4s %-30s %-12s %s\n",  //timestamp, id, event_name, thread_name, event_string
    //.logging_syntax                 = "[%s] [%s] [%s] (thread %s) %s\n", //timestamp, id, event_name, thread_name, event_string
    .non_applicable_log_id_string   = "N/A",
    .log_to_file                    = true,
    .log_to_stdout                  = true,
    .flush_every_n                  = 1
};

#define HELP_MODE_STRING "-h"
#define FILE_MODE_STRING "-f"
#define STOP_MODE_STRING "-z"
#define STOP_SERVER_AND_CLIENT_MODE_STRING "-S"


#endif


