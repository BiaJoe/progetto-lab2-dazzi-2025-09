#ifndef DEBUG_H
#define DEBUG_H

#include "log.h"

#define DEBUG(m) log_event(AUTOMATIC_LOG_ID, DEBUG, m " | line: %d | file: %s ", __LINE__, __FILE__)

#endif