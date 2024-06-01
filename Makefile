free_application_context: free_application_context.c args.h
	gcc -Wall -c free_application_context.o free_application_context.c  

parse_args: parse_args.c args.h
	gcc -Wall -c parse_args.o parse_args.c

object:
	make free_application_context
	make parse_args

clean-object:
	- rm *.o

clean-out:
	- rm *.out

clean: 
	make clean-object
	make clean-out

build:
	make object
	gcc -Wall -g -o main.out main.c *.o -pthread
	make clean-object
