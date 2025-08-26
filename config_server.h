#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include "structs.h"
#include "log.h"

// config file nello stile dei software suckless
// qui vengono definite le configurazioni di default del server

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

#define EMERGENCY_REQUESTS_ARGUMENT_SEPARATOR_CHAR ' '



static const struct timespec server_tick_time = {
	.tv_sec = 1,
	.tv_nsec = 0
};

static const priority_rule_t priority_lookup_table[] = {
//   priority number        tempo prima della promozione,       tempo prima di andare in timeout
    {0,                     120,                                NO_TIMEOUT  },
    {1,                     NO_PROMOTION,                       30          },
    {2,                     NO_PROMOTION,                       10          }
};

const int priority_count = (int)(sizeof(priority_lookup_table)/sizeof(priority_lookup_table[0]));



#endif





