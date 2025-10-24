# Define the C compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Define the targets to be built by default
.PHONY: all
all: part1 empty

# Rule to build the 'part1' executable
part1: part1.c
	$(CC) $(CFLAGS) -o part1 part1.c

# Rule to build the 'empty' executable
empty: empty.c
	$(CC) $(CFLAGS) -o empty empty.c

# Rule to clean up all generated files
.PHONY: clean
clean:
	rm -f part1 empty
