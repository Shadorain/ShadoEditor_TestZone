CC=gcc
SHELL=/bin/sh
CFLAGS=-g -Wno-deprecated -Wall -Wextra -pedantic -std=c99 -pie -pedantic -static-libasan # -fsanitize=address

shado: shado.c rope.c
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o ./shado

valgrind: shado
	valgrind -s --log-file=./.valgrind.log --leak-check=full --show-leak-kinds=all --track-origins=yes ./shado shado.c

gdb: shado
	gdb ./shado

.PHONY: clean valgrind gdb
