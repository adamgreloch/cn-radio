#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "err.h"
#include "pack_buffer.h"
#include "opts.h"

uint64_t curr_session_id;
uint64_t last_session_id = 0;

pack_buffer *audio_pack_buffer;

size_t receive_pack(int socket_fd, struct audio_pack **pack, byte *buffer,
                    uint64_t *psize, uint64_t bsize) {
    size_t read_length;
    int flags = 0;
    errno = 0;

    memset(buffer, 0, bsize);
    read_length = recv(socket_fd, buffer, bsize, flags);

    *psize = read_length - sizeof(struct audio_pack);

    *pack = (struct audio_pack *) buffer;
    (*pack)->audio_data = buffer + sizeof(struct audio_pack);

    curr_session_id = ntohll((*pack)->session_id);

    if (curr_session_id > last_session_id)
        pb_reset(audio_pack_buffer, *psize, ntohll((*pack)->first_byte_num));

    if (curr_session_id < last_session_id)
        return 0;

    last_session_id = curr_session_id;

    return read_length;
}

int main(int argc, char **argv) {
    receiver_opts *opts = get_receiver_opts(argc, argv);

    uint16_t port = opts->port;
    uint64_t bsize = opts->bsize;
    char *from_addr = opts->from_addr;

    byte *buffer = malloc(bsize);
    if (!buffer)
        fatal("malloc");

    int socket_fd = bind_socket(port);

    struct audio_pack *pack;
    size_t read_length;

    audio_pack_buffer = pb_init(bsize);

    byte *write_buffer = malloc(bsize);
    if (!write_buffer)
        fatal("malloc");

    uint64_t psize;

    while (true) {
        read_length = receive_pack(socket_fd, &pack, buffer, &psize,
                                   bsize);

        if (read_length > 0)
            pb_push_back(audio_pack_buffer, ntohll(pack->first_byte_num),
                         pack->audio_data, psize);

        memset(write_buffer, 0, bsize);

        pb_pop_front(audio_pack_buffer, write_buffer, psize);

        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }
}
