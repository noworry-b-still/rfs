# Compiler
CC = gcc

# Targets
SERVER = server
CLIENT = client

# Default target
all: $(SERVER) $(CLIENT)

# Compile server
$(SERVER): server.c
	$(CC) -o $(SERVER) server.c -pthread

# Compile client
$(CLIENT): client.c
	$(CC) -o $(CLIENT) client.c

# Clean up
clean:
	rm -f $(SERVER) $(CLIENT)
	rm -rf server_root

# Phony targets
.PHONY: all clean
