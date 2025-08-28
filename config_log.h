
#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

#include <stdbool.h>
#include "log.h"


//lookup table per ogni evento
const log_event_info_t LOG_EVENT_LOOKUP_TABLE[LOG_EVENT_TYPES_COUNT] = {
//  TYPE											// STRING										// CODICE			//TERMINA?		// DA LOGGARE?
    [NON_APPLICABLE]                    		= { "NON_APPLICABLE",                   			" ",    			false, 			true },
    [DEBUG]                             		= { "DEBUG",                            			"DEH ",  			false, 			true },
    [FATAL_ERROR]                       		= { "FATAL_ERROR",                      			"ferr",  			true, 			true },
    [FATAL_ERROR_CLIENT]                       	= { "FATAL_ERROR_CLIENT",                      		"ferc",  			false, 			true },
	[FATAL_ERROR_PARSING]               		= { "FATAL_ERROR_PARSING",              			"fepa",  			true, 			true },
    [FATAL_ERROR_LOGGING]               		= { "FATAL_ERROR_LOGGING",              			"felo",  			true, 			true },
    [FATAL_ERROR_MEMORY]                		= { "FATAL_ERROR_MEMORY",               			"feme",  			true, 			true },
    [FATAL_ERROR_FILE_OPENING]          		= { "FATAL_ERROR_FILE_OPENING",         			"fefo",  			true, 			true },
    [EMPTY_CONF_LINE_IGNORED]           		= { "EMPTY_CONF_LINE_IGNORED",          			"ecli",  			false, 			true },
    [DUPLICATE_RESCUER_REQUEST_IGNORED] 		= { "DUPLICATE_RESCUER_REQUEST_IGNORED",			"drri",  			false, 			true },
    [WRONG_RESCUER_REQUEST_IGNORED]     		= { "WRONG_RESCUER_REQUEST_IGNORED",    			"wrri",  			false, 			true },
    [DUPLICATE_EMERGENCY_TYPE_IGNORED]  		= { "DUPLICATE_EMERGENCY_TYPE_IGNORED", 			"deti",  			false, 			true },
    [DUPLICATE_RESCUER_TYPE_IGNORED]    		= { "DUPLICATE_RESCUER_TYPE_IGNORED",   			"drti",  			false, 			true },
    [WRONG_EMERGENCY_REQUEST_IGNORED_CLIENT] 	= { "WRONG_EMERGENCY_REQUEST_CLIENT",	        	"werc", 			false, 			true },
    [WRONG_EMERGENCY_REQUEST_IGNORED_SERVER] 	= { "WRONG_EMERGENCY_REQUEST_SERVER",		        "wers", 			false, 			true },
    [LOGGING_STARTED]                   		= { "LOGGING_STARTED",                  			"lsta",  			false,  		true },
    [LOGGING_ENDED]                     		= { "LOGGING_ENDED",                    			"lend",  			true,  			true },
    [PARSING_STARTED]                   		= { "PARSING_STARTED",                  			"psta",  			false,  		true },
    [PARSING_ENDED]                     		= { "PARSING_ENDED",                    			"pend",  			false,  		true },
    [RESCUER_TYPE_PARSED]               		= { "RESCUER_TYPE_PARSED",              			"rtpa",  			false,  		true },
    [RESCUER_DIGITAL_TWIN_ADDED]        		= { "RESCUER_DIGITAL_TWIN_ADDED",       			"rdta",  			false,  		true },
    [EMERGENCY_PARSED]                  		= { "EMERGENCY_PARSED",                 			"empa",  			false,  		true },
    [RESCUER_REQUEST_ADDED]             		= { "RESCUER_REQUEST_ADDED",            			"rrad",  			false,  		true },
    [SERVER_UPDATE]                     		= { "SERVER_UPDATE",                    			"seup",  			false,  		true },
    [SERVER]                            		= { "SERVER",                           			"srvr",  			false,  		true },
    [CLIENT]                            		= { "CLIENT",                           			"clnt",  			false,  		true },
    [EMERGENCY_REQUEST_RECEIVED]        		= { "EMERGENCY_REQUEST_RECEIVED",       			"errr",  			false,  		true },
    [EMERGENCY_REQUEST_PROCESSED]       		= { "EMERGENCY_REQUEST_PROCESSED",      			"erpr",  			false,  		true },
    [MESSAGE_QUEUE_CLIENT]              		= { "MESSAGE_QUEUE_CLIENT",             			"mqcl",  			false,  		true },
    [MESSAGE_QUEUE_SERVER]              		= { "MESSAGE_QUEUE_SERVER",             			"mqse",  			false,  		true },
    [EMERGENCY_STATUS]                  		= { "EMERGENCY_STATUS",                 			"esta",  			false,  		true },
    [RESCUER_STATUS]                    		= { "RESCUER_STATUS",                   			"rsta",  			false,  		true },
    [RESCUER_TRAVELLING_STATUS]         		= { "RESCUER_TRAVELLING_STATUS",        			"rtst",  			false,  		true },
    [EMERGENCY_REQUEST]                 		= { "EMERGENCY_REQUEST",                			"erre",  			false,  		true },
    [PROGRAM_ENDED_SUCCESSFULLY]        		= { "PROGRAM_ENDED_SUCCESSFULLY",       			"pesu",  			true, 			true }
};			

#endif

