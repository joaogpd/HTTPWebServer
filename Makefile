sock: sock/sock.c
	gcc -Wall -c sock.o sock/sock.c

arena: arena/arena.c
	gcc -Wall -c arena.o arena/arena.c

thread_pool: thread_pool/thread_pool.c
	gcc -Wall -c thread_pool.o thread_pool/thread_pool.c

object: 
	make sock
	make arena
	make thread_pool

main:
	make object
	gcc -Wall -o main.out main.c *.o
	./main.out
	make clean
	
gdb:
	make object
	gcc -Wall -g -o main.out main.c *.o
	gdb main.out
	make clean

valgrind:
	make object
	gcc -Wall -g -o main.out main.c *.o
	valgrind -s --leak-check=full --show-leak-kinds=all --track-origins=yes ./main.out
	make clean

helgrind:
	make object
	gcc -Wall -g -o main.out main.c *.o
	valgrind -s --tool=helgrind ./main.out
	make clean

clean:
	rm *.o
	rm *.out