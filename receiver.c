#include <netinet/in.h>
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

void print_debug(struct audio_pack *pack) {
    fprintf(stderr, "\e[1;1H\e[2J");
    fprintf(stderr, "\nfirst_byte_num=%lu\n", ntohll(pack->first_byte_num));
    fprintf(stderr, "psize=%u\n", psize);
    fprintf(stderr, "session_id=%lu\n", ntohll(pack->session_id));
}


uint64_t curr_session_id;
uint64_t last_session_id = 0;

pack_buffer *audio_pack_buffer;

size_t receive_pack(int socket_fd, struct audio_pack **pack, byte *buffer) {
    size_t read_length;
    int flags = 0;
    errno = 0;

    memset(buffer, 0, BUFFER_SIZE);
    read_length = recv(socket_fd, buffer, BUFFER_SIZE, flags);

    psize = read_length - sizeof(struct audio_pack);

    *pack = (struct audio_pack *) buffer;
    (*pack)->audio_data = buffer + sizeof(struct audio_pack);

    curr_session_id = ntohll((*pack)->session_id);

    if (curr_session_id > last_session_id)
        pb_reset(audio_pack_buffer, psize, ntohll((*pack)->first_byte_num),
                 curr_session_id);

    if (curr_session_id < last_session_id)
        return 0;

    last_session_id = curr_session_id;

    return read_length;
}

void *gather_packs() {
    byte *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
        fatal("malloc");

    int socket_fd = bind_socket(port);

    struct audio_pack *pack;
    size_t read_length;

    while (true) {
        read_length = receive_pack(socket_fd, &pack, buffer);

        if (read_length > 0)
            pb_push_back(audio_pack_buffer, ntohll(pack->first_byte_num),
                         pack->audio_data, psize);
    }
}

void *print_packs() {
    byte *write_buffer = malloc(BUFFER_SIZE);
    if (!write_buffer)
        fatal("malloc");

    while (true) {
        memset(write_buffer, 0, BUFFER_SIZE);
        pb_pop_front(audio_pack_buffer, write_buffer, psize);
        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }
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
