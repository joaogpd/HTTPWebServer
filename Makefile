sock: sock/sock.c
	gcc -Wall -g -c sock.o sock/sock.c -lpthread

arena: arena/arena.c
	gcc -Wall -g -c arena.o arena/arena.c -lpthread

thread_pool: thread_pool/thread_pool.c
	gcc -Wall -g -c thread_pool.o thread_pool/thread_pool.c -lphtread

filewriter: filewriter/filewriter.c
	gcc -Wall -g -c filewriter.o filewriter/filewriter.c -lphtread

object: 
	make sock
	make arena
	make thread_pool
	make filewriter

main:
	make object
	gcc -Wall -o main.out main.c *.o
	rm *.o
	./main.out
	make clean

build:
	make object
	gcc -Wall -g -o main.out main.c *.o -lpthread
	rm *.o
	
gdb:
	make object
	gcc -Wall -g -o main.out main.c *.o
	rm *.o
	gdb main.out
	make clean

valgrind:
	make object
	gcc -Wall -g -o main.out main.c *.o
	rm *.o
	valgrind -s --leak-check=full --show-leak-kinds=all --track-origins=yes ./main.out
	make clean

helgrind:
	make object
	gcc -Wall -g -o main.out main.c *.o
	rm *.o
	valgrind -s --tool=helgrind ./main.out
	make clean

clean: 
	- rm *.o 
	- rm *.out