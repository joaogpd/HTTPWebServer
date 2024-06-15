OBJECTS=main.o args.o clients.o file_handler.o server.o client_thread.o start_server.o
EXEC=webserver.out

# runs clean if there is an error
all:
	make build || make clean

build: 
	make object
	gcc -Wall -o $(EXEC) $(OBJECTS) -pthread
	make clean-object

args: args.c
	gcc -Wall -c args.o args.c

clients: clients.c
	gcc -Wall -c clients.o clients.c -pthread

file_handler: file_handler.c
	gcc -Wall -c file_handler.o file_handler.c -pthread

client_thread: client_thread.c
	gcc -Wall -c client_thread.o client_thread.c -pthread

start_server: start_server.c
	gcc -Wall -c start_server.o start_server.c -pthread

server: server.c
	gcc -Wall -c server.o server.c -pthread

main: main.c
	gcc -Wall -c main.o main.c -pthread

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
