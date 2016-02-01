C=gcc
CFLAGS=-Wall -W -g -Werror 

all: client server

client: client.c raw.c
	$(CC) client.c $(CFLAGS) -o client

server: server.c 
	$(CC) server.c -o server

clean:
	rm -f client server *.o


