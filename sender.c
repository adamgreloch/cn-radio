#include <netinet/in.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "err.h"
#include "common.h"
#include "opts.h"

char *dest_addr;
uint16_t port;
uint32_t psize;

char *send_buffer;

size_t read_pack(FILE *stream, uint64_t pack_size, byte *data) {
    return fread(data, sizeof(byte), pack_size, stream);
}

void send_pack(int socket_fd, const struct sockaddr_in *dest_address,
               const struct audio_pack *pack, size_t _psize) {
    socklen_t address_length = (socklen_t) sizeof(*dest_address);
    int flags = 0;

    ssize_t data_size = _psize + 16;

    memset(send_buffer, 0, data_size);

    memcpy(send_buffer, &pack->session_id, 8);
    memcpy(send_buffer + 8, &pack->first_byte_num, 8);
    memcpy(send_buffer + 16, pack->audio_data, _psize);

    ssize_t sent_size = sendto(socket_fd, send_buffer, data_size, flags,
                               (struct sockaddr *) dest_address,
                               address_length);

    ENSURE(sent_size == data_size);
}

void *pack_sender() {
    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server_address = get_send_address(dest_addr, port);

    enable_multicast(socket_fd, &server_address);

    uint64_t session_id = time(NULL);

    uint64_t pack_num = 0;

    byte *read_bytes = (byte *) malloc(psize);

    while (!feof(stdin)) {
        memset(read_bytes, 0, psize);

        if (psize == read_pack(stdin, psize, read_bytes)) {
            struct audio_pack pack;

            pack.session_id = htonll(session_id);
            pack.first_byte_num = htonll(pack_num * psize);
            pack.audio_data = read_bytes;

            send_pack(socket_fd, &server_address, &pack, psize);

            pack_num++;
        } else break;
    }

    free(read_bytes);

    CHECK_ERRNO(close(socket_fd));

    return 0;
}

int main(int argc, char **argv) {
    sender_opts *opts = get_sender_opts(argc, argv);

    port = opts->port;
    psize = opts->psize;

    send_buffer = calloc(psize + 16, 1);
    if (!send_buffer)
        fatal("calloc");

    check_address(opts->dest_addr);
    dest_addr = opts->dest_addr;

    pthread_t sender;

    CHECK_ERRNO(pthread_create(&sender, NULL, pack_sender, NULL));

    CHECK_ERRNO(pthread_join(sender, NULL));

    free(send_buffer);
    free(opts);

    return 0;
}
