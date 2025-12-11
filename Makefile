CC = gcc
CFLAGS = -Wall -Wextra -Ishared -Iserver -Iclient

CLIENT_SRC = client/client.c shared/protocol.c
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
CLIENT_BIN = client_bin

SERVER_SRC = server/server.c server/client_handler.c server/quiz.c server/logger.c shared/protocol.c
SERVER_OBJ = $(SERVER_SRC:.c=.o)
SERVER_BIN = server_bin

.PHONY: all clean run_client run_server

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC)

$(SERVER_BIN):	$(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC)

run_client:	$(CLIENT_BIN)
	./$(CLIENT_BIN) 8080

run_server:	$(SERVER_BIN)
	./$(SERVER_BIN)

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN) $(CLIENT_OBJ) $(SERVER_OBJ)