# Diretórios
BIN_DIR = ./bin

# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = 

# Arquivos de saída
CLIENT_BIN = $(BIN_DIR)/client
SERVER_BIN = $(BIN_DIR)/server

# Fontes e objetos do Client
CLIENT_SOURCES = ./client.c
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

# Fontes e objetos do Server
SERVER_SOURCES = ./server.c
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)

# Regras principais
all: $(CLIENT_BIN) $(SERVER_BIN)

# Regra para compilar o Client
$(CLIENT_BIN): $(CLIENT_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Regra para compilar o Server
$(SERVER_BIN): $(SERVER_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Regras de compilação para objetos
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza
clean:
	rm -rf $(BIN_DIR)/*.o $(CLIENT_OBJECTS) $(SERVER_OBJECTS)

cleanall: clean
	rm -rf $(CLIENT_BIN) $(SERVER_BIN)

.PHONY: all clean cleanall
