build:
	make object
	gcc -Wall -o webserver.out main.c *.o -pthread
	make clean-object

args: args.c
	gcc -Wall -c args.o args.c

clients: clients.c
	gcc -Wall -c clients.o clients.c -pthread

file_handler: file_handler.c
	gcc -Wall -c file_handler.o file_handler.c -pthread

server: server.c
	gcc -Wall -c server.o server.c -pthread

object: args.o file_handler.o clients.o server.o
	make args
	make file_handler
	make clients
	make server

clean-object:
	- rm *.o

clean-out:
	- rm *.out

clean: 
	make clean-object
	make clean-out
