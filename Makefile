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
	make clean

clean:
	rm *.o
	# rm *.out