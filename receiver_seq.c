#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "err.h"
#include "pack_buffer_seq.h"

#define BUFFER_SIZE 20000

uint16_t port = DEFAULT_PORT;       // UDP data port
uint64_t bsize = DEFAULT_BSIZE;     // buffer size
uint64_t psize = DEFAULT_PSIZE;

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
        pb_reset(audio_pack_buffer, psize, ntohll((*pack)->first_byte_num));

    if (curr_session_id < last_session_id)
        return 0;

    last_session_id = curr_session_id;

    return read_length;
}

int main() {
    byte *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
        fatal("malloc");

    int socket_fd = bind_socket(port);

    struct audio_pack *pack;
    size_t read_length;

    audio_pack_buffer = pb_init(bsize);

    byte *write_buffer = malloc(BUFFER_SIZE);
    if (!write_buffer)
        fatal("malloc");

    while (true) {
        read_length = receive_pack(socket_fd, &pack, buffer);

        if (read_length > 0)
            pb_push_back(audio_pack_buffer, ntohll(pack->first_byte_num),
                         pack->audio_data, psize);

        memset(write_buffer, 0, BUFFER_SIZE);

        pb_pop_front(audio_pack_buffer, write_buffer, psize);

        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }
}
