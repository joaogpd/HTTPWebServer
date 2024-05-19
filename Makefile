sock: sock/sock.c
	gcc -Wall -c sock.o sock/sock.c

arena: arena/arena.c
	gcc -Wall -c arena.o arena/arena.c

object: 
	make sock
	make arena
	make clean

clean:
	rm *.o
	rm *.out