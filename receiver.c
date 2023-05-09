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

uint64_t curr_session_id;
uint64_t last_session_id = 0;

pack_buffer *audio_pack_buffer;
uint16_t port;
uint64_t bsize;
struct sockaddr_in listening_address;

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

    enable_multicast(socket_fd, &listening_address);

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

int main(int argc, char **argv) {
    receiver_opts *opts = get_receiver_opts(argc, argv);

    port = opts->port;
    bsize = opts->bsize;
    listening_address = parse_host_and_port(opts->from_addr, opts->portstr);

    audio_pack_buffer = pb_init(bsize);

    pthread_t receiver;
    pthread_t printer;

    CHECK_ERRNO(pthread_create(&receiver, NULL, pack_receiver, NULL));
    CHECK_ERRNO(pthread_create(&printer, NULL, pack_printer, NULL));

    CHECK_ERRNO(pthread_join(receiver, NULL));
    CHECK_ERRNO(pthread_join(printer, NULL));

    return 0;
}
