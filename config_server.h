#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include "structs.h"
#include "log.h"

// config file nello stile dei software suckless
// qui vengono definite le configurazioni di default del server

#define RESCUERS_CONF        "conf/rescuers.conf"
#define EMERGENCY_TYPES_CONF "conf/emergency_types.conf"
#define ENV_CONF             "conf/env.conf"

// file per facilitare il dibugging
#define SIMULATION_JSON  "out/simulation.json"
#define POSITIONS_JSON   "out/positions.json"
#define POSITIONS_TXT    "out/positions.txt"

static const logging_config_t server_logging_config = {
    .log_file                       = "log.txt",
    .log_file_ptr                   = NULL,
    .logging_syntax                 = "[%s] [%s] [%s] (%s) %s\n", //timestamp, id, event_name, thread_name, event_string
    //.logging_syntax                 = "%-13s %-4s %-30s %-12s %s\n",  //timestamp, id, event_name, thread_name, event_string
    .non_applicable_log_id_string   = "N/A",
    .log_to_file                    = true,
    .log_to_stdout                  = true,
    .flush_every_n                  = 2
};

#define EMERGENCY_REQUESTS_ARGUMENT_SEPARATOR_CHAR ' '

#define ZERO_POINT_50_SECONDS 500000000
#define ZERO_POINT_25_SECONDS 250000000
#define ZERO_POINT_10_SECONDS 100000000
#define ZERO_POINT_01_SECONDS 10000000

static const struct timespec server_tick_time = { // cambiare se si vogliono mettere dei valori manualmente
    .tv_sec  = TICK_SEC,     
    .tv_nsec = TICK_NSEC
};

static const priority_rule_t priority_lookup_table[] = {
//   priority number        tempo prima della promozione,       tempo prima di andare in timeout
    {0,                     60,                                NO_TIMEOUT  },
    {1,                     NO_PROMOTION,                       60          },
    {2,                     NO_PROMOTION,                       30          }
};

#endif





