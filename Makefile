TARGETS = sikradio-receiver sikradio-sender

CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -g

all: $(TARGETS)

# TODO add headers

pack_buffer.o: pack_buffer.c

ctrl_protocol.o: ctrl_protocol.c

receiver_ui.o: receiver_ui.c

rexmit_queue.o: rexmit_queue.c

sikradio-receiver: pack_buffer.o ctrl_protocol.o receiver_ui.o receiver.c
	$(CC) $^ -o $@ $(CFLAGS)

sikradio-sender: ctrl_protocol.o rexmit_queue.o sender.c
	$(CC) $^ -o $@ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(TARGETS) *.o *~ 
