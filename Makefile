# ==========================
#  Makefile semplice (liste)
# ==========================

# ---- Toolchain e flag
CC      ?= gcc
STD     ?= c11
WARN    ?= -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter
OPT     ?= -O2
DBG     ?= -g
CPPFLAGS?= -MMD -MP -Iinclude -I.       
CFLAGS  ?= -std=$(STD) $(OPT) $(DBG) $(WARN)
LDLIBS  ?= -pthread -lrt -lm             

CFLAGS += -D_POSIX_C_SOURCE=200809L

BIN_DIR := bin

# ---- Sorgenti comuni (libreria di progetto)
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

# ---- Server
SERVER_SRCS := \
  src/server/main.c \
  src/server/thread_functions/server_updater.c \
  src/server/thread_functions/thread_clock.c \
  src/server/thread_functions/thread_reciever.c \
  src/server/thread_functions/thread_worker.c

# ---- Client
CLIENT_SRCS := \
  src/client/client.c

# ---- Oggetti (compilazione in-place semplice)
COMMON_OBJS := $(COMMON_SRCS:.c=.o)
SERVER_OBJS := $(SERVER_SRCS:.c=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

# ---- Eseguibili
SERVER_BIN := /server
CLIENT_BIN := /client

# ---- Target principali
.PHONY: all clean distclean run tests

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(COMMON_OBJS) $(SERVER_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(CLIENT_BIN): $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

# ---- Run: solo server
run: $(SERVER_BIN)
	@echo "▶ Avvio server…"
	@$(SERVER_BIN)

# ---- Compilazione .o + deps
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ---- Pulizia
clean:
	@$(RM) $(COMMON_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS)
	@$(RM) $(COMMON_OBJS:.o=.d) $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)
	@$(RM) log.txt
	@echo "Puliti oggetti e deps."

distclean: clean
	@$(RM) $(CLIENT_BIN) $(SERVER_BIN)
	@echo "Puliti eseguibili."

# ---- Dipendenze auto-generate
DEPS := $(COMMON_OBJS:.o=.d) $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)
-include $(DEPS)

# ---- (Opzionale) build dei test singoli:
# Aggiungi qui per ciascun test che vuoi (linkato con COMMON_OBJS)
# Esempi:
# bin/tests/log_test: src/tests/log_test.c $(COMMON_OBJS)
# 	@mkdir -p bin/tests
# 	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
#
# tests: bin/tests/log_test bin/tests/log_stress bin/tests/test_queue