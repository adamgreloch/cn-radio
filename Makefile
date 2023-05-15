TARGETS = sikradio-receiver sikradio-sender

CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

all: $(TARGETS)

pack_buffer.o: common.h pack_buffer.h pack_buffer.c

ctrl_protocol.o: ctrl_protocol.c

sikradio-receiver: pack_buffer.o ctrl_protocol.o opts.h err.h common.h receiver.c 
	$(CC) $^ -o $@ $(CFLAGS)

sikradio-sender: opts.h ctrl_protocol.o err.h common.h sender.c 
	$(CC) $^ -o $@ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(TARGETS) *.o *~ 
