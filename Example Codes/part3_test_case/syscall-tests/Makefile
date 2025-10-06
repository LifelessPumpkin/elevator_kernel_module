CC := gcc
CFLAGS := -Wall -I../
EXECS := producer consumer

all: $(EXECS)
 
producer: producer.c ../wrappers.h
	$(CC) $(CFLAGS) -o $@ $<

consumer: consumer.c ../wrappers.h
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean all

clean: 
	rm -f $(EXECS)