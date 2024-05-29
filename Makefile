build:
	gcc -Wall -o main main.c -pthread

clean: 
	- rm *.o 
	- rm *.out