build:
	gcc -Wall -g -o main main.c -pthread

clean: 
	- rm *.o 
	- rm *.out