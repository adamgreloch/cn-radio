#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"
#include "err.h"
#include "pack_buffer.h"

#define BUFFER_SIZE 20000

uint16_t port = DEFAULT_PORT;       // UDP data port
uint64_t bsize = DEFAULT_BSIZE;     // buffer size
uint64_t psize = DEFAULT_PSIZE;

void print_debug(struct sockaddr_in *client_address, struct audio_pack
*pack,
                 uint64_t read_length) {
    fprintf(stderr, "\e[1;1H\e[2J");
    fprintf(stderr, "\nfirst_byte_num=%lu\n", ntohll(pack->first_byte_num));
    fprintf(stderr, "psize=%u\n", psize);
    fprintf(stderr, "session_id=%lu\n", ntohll(pack->session_id));

    char *client_ip = inet_ntoa(client_address->sin_addr);
    uint16_t client_port = ntohs(client_address->sin_port);

    fprintf(stderr, "received %zd bytes from client %s:%u\n",
            read_length, client_ip, client_port);
}


uint64_t curr_session_id;
uint64_t last_session_id = 0;

pack_buffer *audio_pack_buffer;

size_t receive_pack(int socket_fd, struct audio_pack **pack, byte *buffer) {
    struct sockaddr_in client_address;
    size_t read_length;

    socklen_t address_length = (socklen_t) sizeof(client_address);

    int flags = 0;

    errno = 0;

    memset(buffer, 0, BUFFER_SIZE);

    read_length = recvfrom(socket_fd, buffer, BUFFER_SIZE,
                           flags, (struct sockaddr *) &client_address,
                           &address_length);

    psize = read_length - sizeof(struct audio_pack);

    *pack = (struct audio_pack *) buffer;
    (*pack)->audio_data = buffer + sizeof(struct audio_pack);

    curr_session_id = ntohll((*pack)->session_id);

    if (curr_session_id > last_session_id)
        pb_reset(audio_pack_buffer, psize);

    last_session_id = curr_session_id;

    print_debug(&client_address, *pack, read_length);

    return read_length;
}

void *gather_packs() {
    byte *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
        fatal("malloc");

    int socket_fd = bind_socket(port);

    struct audio_pack *pack;
    size_t read_length;

    do {
        read_length = receive_pack(socket_fd, &pack, buffer);
        pb_push_back(audio_pack_buffer, ntohll(pack->first_byte_num),
                     pack->audio_data, psize);
    } while (read_length > 0);

    CHECK_ERRNO(close(socket_fd));

    return 0;
}

void *print_packs() {
    byte *write_buffer = calloc(BUFFER_SIZE, 1);
    if (!write_buffer)
        fatal("calloc");

    while (true) {
        pb_pop_front(audio_pack_buffer, write_buffer, psize);
        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }

    return 0;
}

int main() {
    audio_pack_buffer = pb_init(bsize);

    pthread_t gatherer_id;
    pthread_t printer_id;

    CHECK_ERRNO(pthread_create(&gatherer_id, NULL, gather_packs, NULL));
    CHECK_ERRNO(pthread_create(&printer_id, NULL, print_packs, NULL));

    CHECK_ERRNO(pthread_join(gatherer_id, NULL));
    CHECK_ERRNO(pthread_join(printer_id, NULL));

    pb_free(audio_pack_buffer);

    return 0;
}
