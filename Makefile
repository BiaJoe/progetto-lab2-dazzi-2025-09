# ==========================
#  Makefile semplice (liste)
# ==========================

# ---- Toolchain e flag
CC       ?= gcc
STD      ?= c11
WARN     ?= -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter
OPT      ?= -O2
DBG      ?= -g
CPPFLAGS ?= -MMD -MP -Iinclude -I.
CFLAGS   ?= -std=$(STD) $(OPT) $(DBG) $(WARN)
LDLIBS   ?= -pthread -lrt -lm

# POSIX features
CFLAGS  += -D_POSIX_C_SOURCE=200809L

# ---- Cartelle / strumenti
BIN_DIR    := bin
PYTHON     ?= python3
TOOLS_DIR  := tools
OUT_DIR    := out

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
  src/server/server_helpers.c \
  src/server/thread_functions/json_visualizer.c \
  src/server/thread_functions/thread_updater.c \
  src/server/thread_functions/thread_clock.c \
  src/server/thread_functions/thread_reciever.c \
  src/server/thread_functions/thread_worker.c

# ---- Client
CLIENT_SRCS := \
  src/client/client.c

# ---- Oggetti
COMMON_OBJS := $(COMMON_SRCS:.c=.o)
SERVER_OBJS := $(SERVER_SRCS:.c=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

# ---- Eseguibili
SERVER_BIN := ./server
CLIENT_BIN := ./client

# ---- Artefatti simulazione
SIM_JSON     := $(OUT_DIR)/sim.json
FRAMES_DIR   := $(OUT_DIR)/frames
FRAMES_SCRIPT:= $(TOOLS_DIR)/json2frames.py

# ---- Target principali
.PHONY: all clean distclean run tests frames frames-clean

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(COMMON_OBJS) $(SERVER_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(CLIENT_BIN): $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

# ---- Run: solo server
run: $(SERVER_BIN) $(CLIENT_BIN)
	@echo "▶ Avvio server…"
	@$(SERVER_BIN)

.PHONY: show-pos show_pos

show_pos: out/positions.txt

show-pos show_pos: out/positions.txt
	@echo "==== out/positions.txt ===="
	@cat out/positions.txt


# ---- Compilazione .o + deps
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ---- Pulizia
clean: 
	@$(RM) $(COMMON_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS)
	@$(RM) $(COMMON_OBJS:.o=.d) $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)
	@$(RM) $(CLIENT_BIN) $(SERVER_BIN)
	@echo "Puliti oggetti, deps ed eseguibili."


# ---- Dipendenze auto-generate
DEPS := $(COMMON_OBJS:.o=.d) $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)
-include $(DEPS)