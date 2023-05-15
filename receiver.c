#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"
#include "err.h"
#include "pack_buffer.h"
#include "opts.h"
#include "ctrl_protocol.h"

uint64_t curr_session_id;
uint64_t last_session_id = 0;

pack_buffer *audio_pack_buffer;
uint16_t ctrl_port;
uint16_t port;
uint64_t bsize;
struct sockaddr_in listening_addr;
struct sockaddr_in discover_addr;

#define DISCOVER_SLEEP 5

size_t receive_pack(int socket_fd, struct audio_pack **pack, byte *buffer,
                    uint64_t *psize) {
    size_t read_length;
    int flags = 0;
    errno = 0;

    memset(buffer, 0, bsize);

    struct sockaddr_in client_address;
    socklen_t address_length = (socklen_t) sizeof(client_address);

    read_length = recvfrom(socket_fd, buffer, bsize, flags, (struct sockaddr
    *) &client_address, &address_length);

    // FIXME radio stations may interfere on one multicast address

    *psize = read_length - 16;

    memcpy(&(*pack)->session_id, buffer, 8);
    memcpy(&(*pack)->first_byte_num, buffer + 8, 8);
    (*pack)->audio_data = buffer + 16;

    curr_session_id = ntohll((*pack)->session_id);

    if (curr_session_id > last_session_id)
        pb_reset(audio_pack_buffer, *psize, ntohll((*pack)->first_byte_num));

    if (curr_session_id < last_session_id)
        return 0;

    last_session_id = curr_session_id;

    return read_length;
}

void *pack_receiver() {
    uint64_t psize;

    struct audio_pack *pack = malloc(sizeof(struct audio_pack));
    size_t read_length;

    int socket_fd = bind_socket(port);

    enable_multicast(socket_fd, &listening_addr);

    byte *buffer = malloc(bsize);
    if (!buffer)
        fatal("malloc");

    while (true) {
        read_length = receive_pack(socket_fd, &pack, buffer, &psize);

        if (read_length > 0)
            pb_push_back(audio_pack_buffer, ntohll(pack->first_byte_num),
                         pack->audio_data, psize);
    }
}

void *pack_printer() {
    byte *write_buffer = malloc(bsize);
    if (!write_buffer)
        fatal("malloc");

    uint64_t psize;

    while (true) {
        memset(write_buffer, 0, bsize);
        psize = pb_pop_front(audio_pack_buffer, write_buffer);
        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }
}

void *station_discoverer() {
    int ctrl_sock_fd = bind_socket(ctrl_port);
    enable_broadcast(ctrl_sock_fd);

    char *write_buffer = malloc(CTRL_BUF_SIZE);
    if (!write_buffer)
        fatal("malloc");

    int wrote_size;
    ssize_t sent_size;
    ssize_t recv_size;
    int flags = 0;
    errno = 0;

    socklen_t discover_addr_len = (socklen_t) sizeof(discover_addr);

    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = (socklen_t) sizeof(sender_addr);

    char mcast_addr_str[20];
    uint16_t sender_port;
    char sender_name[64+1];

    while (true) {
        wrote_size = write_lookup(write_buffer);
        sent_size = sendto(ctrl_sock_fd, write_buffer, wrote_size,
                           flags, (struct sockaddr *)
                                   &discover_addr, discover_addr_len);
        ENSURE(sent_size == wrote_size);

        fprintf(stderr, "sent lookup\n");

        sleep(DISCOVER_SLEEP);

        while ((recv_size = recvfrom(ctrl_sock_fd, write_buffer, CTRL_BUF_SIZE,
                             MSG_DONTWAIT,
                             (struct sockaddr *) &sender_addr,
                             &sender_addr_len)) > 0) {
            if (what_message(write_buffer) == REPLY) {
                parse_reply(write_buffer, recv_size, mcast_addr_str, &sender_port,
                            sender_name);
                fprintf(stderr, "got reply from %s:%d '%s'\n", mcast_addr_str,
                        sender_port, sender_name);
            }
        }
        fprintf(stderr, "no more replies\n");
    }
}

int main(int argc, char **argv) {
    receiver_opts *opts = get_receiver_opts(argc, argv);

    port = opts->port;
    bsize = opts->bsize;
    ctrl_port = opts->ctrl_port;
    listening_addr = parse_host_and_port(opts->mcast_addr, opts->portstr);
    discover_addr = parse_host_and_port(opts->discover_addr, opts->ctrl_portstr);

    audio_pack_buffer = pb_init(bsize);

    pthread_t receiver;
    pthread_t printer;
    pthread_t discoverer;

    CHECK_ERRNO(pthread_create(&receiver, NULL, pack_receiver, NULL));
    CHECK_ERRNO(pthread_create(&printer, NULL, pack_printer, NULL));
    CHECK_ERRNO(pthread_create(&discoverer, NULL, station_discoverer, NULL));

    CHECK_ERRNO(pthread_join(receiver, NULL));
    CHECK_ERRNO(pthread_join(discoverer, NULL));
    CHECK_ERRNO(pthread_join(printer, NULL));

    return 0;
}
