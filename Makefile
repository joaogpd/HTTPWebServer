free_application_context: free_application_context.c application_context.h
	gcc -Wall -c free_application_context.o free_application_context.c  

object:
	make free_application_context

build:
	make object
	gcc -Wall -g -o main main.c *.o -pthread

clean: 
	- rm *.o 
	- rm *.out