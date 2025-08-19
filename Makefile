# Cartelle
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

SRC_DEPS = $(SRC_DIR)/dependencies
SRC_PARSERS = $(SRC_DIR)/parsers

# Compiler e flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I$(INCLUDE_DIR)

# i file necessari per compilare il client e il server
CLIENT_SRC = \
				$(SRC_DIR)/client.c \
				$(SRC_DEPS)/utils.c \
				$(SRC_DEPS)/log.c \

SERVER_SRC = \
				$(SRC_DIR)/server.c \
				$(SRC_DIR)/logger.c \
				$(wildcard $(SRC_DEPS)/*.c) \
				$(wildcard $(SRC_PARSERS)/*.c)

# file binari
CLIENT_BIN = $(BIN_DIR)/client
SERVER_BIN = $(BIN_DIR)/server


# === Regola principale ===
all: $(CLIENT_BIN) $(SERVER_BIN)

# === Compilazione e link diretto ===
$(CLIENT_BIN): $(CLIENT_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(SERVER_BIN): $(SERVER_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

# === Directory di destinazione ===
$(BIN_DIR):
	mkdir -p $@

# === Pulizia ===
clean:
	clear
	rm -rf $(BIN_DIR)
	> log.txt
	

# === Run server in background ===
run: all
	@echo "Running server in background..."
	@$(SERVER_BIN) &

.PHONY: all clean run
