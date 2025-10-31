CC = gcc
CFLAGS = -Wall -g

.PHONY: all
all: part1 empty

part1: part1.c
	$(CC) $(CFLAGS) -o part1 part1.c

empty: empty.c
	$(CC) $(CFLAGS) -o empty empty.c

.PHONY: clean
clean:
	rm -f part1 empty
