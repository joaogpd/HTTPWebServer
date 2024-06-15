OBJECTS=main.o args.o clients.o file_handler.o server.o client_thread.o start_server.o
EXEC=webserver.out
CC-OBJ=gcc -Wall -c
CC=gcc -Wall -o
CC-OBJ-DBG=gcc -Wall -g -c
CC-DBG=gcc -Wall -g -o

# runs clean if there is an error
all:
	make build || make clean

build: 
	make object
	$(CC-DBG) $(EXEC) $(OBJECTS) -pthread
	make clean-object

args: args.c
	$(CC-OBJ-DBG) args.o args.c

clients: clients.c
	$(CC-OBJ-DBG) clients.o clients.c -pthread

file_handler: file_handler.c
	$(CC-OBJ-DBG) file_handler.o file_handler.c -pthread

client_thread: client_thread.c
	$(CC-OBJ-DBG) client_thread.o client_thread.c -pthread

start_server: start_server.c
	$(CC-OBJ-DBG) start_server.o start_server.c -pthread

server: server.c
	$(CC-OBJ-DBG) server.o server.c -pthread

main: main.c
	$(CC-OBJ-DBG) main.o main.c -pthread

object: 
	make args
	make file_handler
	make clients
	make server
	make client_thread
	make start_server
	make main

clean-object:
	- rm $(OBJECTS)

clean-out:
	- rm $(EXEC)

clean: 
	make clean-object
	make clean-out
