CC       = gcc
STD      = c11
WARN     = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter
OPT      = -O2
DBG      = -g
CPPFLAGS = -Iinclude -I.
CFLAGS   = -std=$(STD) $(OPT) $(DBG) $(WARN)
LDLIBS   = -pthread -lrt -lm
CFLAGS   += -D_POSIX_C_SOURCE=200809L

# tick (secondi da 0.01 a 10)
TICK ?= 1.00

# trovato il codice per questo parsing minimale secondi - nanosecondi online

# calcolo 
TICK_SEC  := $(shell awk -v t=$(TICK) 'BEGIN{ s=int(t); printf "%d", s }')
TICK_NSEC := $(shell awk -v t=$(TICK) 'BEGIN{ s=int(t); ns=int((t-s)*1e9 + 0.5); if (ns<0) ns=0; if (ns>999999999) ns=999999999; printf "%d", ns }')



CPPFLAGS += -DTICK_SEC=$(TICK_SEC) -DTICK_NSEC=$(TICK_NSEC)

OUT_DIR  := out

COMMON_SRCS := \
  src/bresenham.c \
  src/log.c \
  src/priority_queue.c \
  src/core/parsers/parse_emergencies.c \
  src/core/parsers/parse_env.c \
  src/core/parsers/parse_rescuers.c \
  src/core/parsers/parsing_utils.c \
  src/core/utils/structs.c \
  src/core/utils/utils.c

# server
SERVER_SRCS := \
  src/server/main.c \
  src/server/server_helpers.c \
  src/server/thread_functions/json_visualizer.c \
  src/server/thread_functions/thread_updater.c \
  src/server/thread_functions/thread_clock.c \
  src/server/thread_functions/thread_reciever.c \
  src/server/thread_functions/thread_worker.c

# client
CLIENT_SRCS := \
  src/client/client.c

# oggetti
COMMON_OBJS := $(COMMON_SRCS:.c=.o)
SERVER_OBJS := $(SERVER_SRCS:.c=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

# binari
SERVER_BIN := ./server
CLIENT_BIN := ./client

.PHONY: all run show-pos show_pos clean distclean help verbose default shy debug

# default
all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(COMMON_OBJS) $(SERVER_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(CLIENT_BIN): $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(SERVER_BIN) $(CLIENT_BIN)
	@echo "▶ Avvio server…"
	@$(SERVER_BIN)

show_pos: out/positions.txt
show-pos show_pos: out/positions.txt
	@echo "==== out/positions.txt ===="
	@cat out/positions.txt

clean:
	@$(RM) $(COMMON_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS)
	@$(RM) $(COMMON_OBJS:.o=.d) $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)
	@$(RM) $(CLIENT_BIN) $(SERVER_BIN)
	@echo "Puliti oggetti, deps ed eseguibili."

# logging
verbose:
	@$(MAKE) CPPFLAGS='$(CPPFLAGS) -DLOG_VERBOSE' all

default:
	@$(MAKE) CPPFLAGS='$(CPPFLAGS)' all

shy:
	@$(MAKE) CPPFLAGS='$(CPPFLAGS) -DLOG_SHY' all

debug:
	@$(MAKE) CPPFLAGS='$(CPPFLAGS) -DLOG_DEBUGGING' all

help:
	@echo ""
	@echo "Comandi:"
	@echo "  make            Compila server e client"
	@echo "  make verbose    Log completo (-DLOG_VERBOSE)"
	@echo "  make default    Log predefinito, come make"
	@echo "  make shy        Log ridotto (-DLOG_SHY)"
	@echo "  make debug      Log di debugging (-DLOG_DEBUGGING)"
	@echo "  make run        Avvia il server (usa TICK=0.01..10)"
	@echo "  make show-pos   Stampa out/positions.txt"
	@echo "  make clean      Pulisce oggetti & eseguibili"
	@echo "  make distclean  clean + rimozione $(OUT_DIR)"
	@echo ""
	@echo "Variabili:"
	@echo "  TICK=<sec>      Tempo per tick (default: 1s), anche decimali ok"
	@echo ""