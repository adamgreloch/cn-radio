TARGETS = sikradio-receiver sikradio-sender

CC     = gcc
CFLAGS = -g -Wall -Wextra -O2 -pthread

all: $(TARGETS)

pack_buffer.o: common.h pack_buffer.h pack_buffer.c

ctrl_protocol.o: opts.h ctrl_protocol.h ctrl_protocol.c

receiver_ui.o: err.h receiver_config.h receiver_utils.h receiver_ui.h ctrl_protocol.h receiver_ui.c

rexmit_queue.o: common.h rexmit_queue.h rexmit_queue.c

sikradio-receiver: receiver_utils.h opts.h common.h err.h pack_buffer.o ctrl_protocol.o receiver_ui.o receiver.c
	$(CC) $^ -o $@ $(CFLAGS)

sikradio-sender: sender_utils.h opts.h common.h err.h ctrl_protocol.o rexmit_queue.o sender.c
	$(CC) $^ -o $@ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(TARGETS) *.o *~ 
