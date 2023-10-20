all: main

main: main.c common.h
	gcc -o main common.c main.c -lm

.PHONY: clean
clean:
	rm -f main
