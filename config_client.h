#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include "client.h"
#include "structs.h"
#include "log.h"

static const logging_config_t client_logging_config = {
    .log_file                       = "log.txt",
    .log_file_ptr                   = NULL,
    .logging_syntax                 = "%-16s %-6s %-35s %s\n",
    .non_applicable_log_id_string   = "N/A",
    .log_to_file                    = true,
    .log_to_stdout                  = true,
    .flush_every_n                  = 1
};

#define FILE_MODE_STRING "-f"
#define STOP_MODE_STRING "-z"
#define STOP_SERVER_AND_CLIENT_MODE_STRING "-S"


#endif


