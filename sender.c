#include <netinet/in.h>
#include <time.h>
#include <unistd.h>
#include "err.h"
#include "common.h"
#include "opts.h"

size_t read_pack(FILE *stream, uint64_t pack_size, byte *data) {
    return fread(data, sizeof(byte), pack_size, stream);
}

void send_pack(int socket_fd, const struct sockaddr_in *dest_address,
               const struct audio_pack *pack, size_t psize) {
    socklen_t address_length = (socklen_t) sizeof(*dest_address);
    int flags = 0;

    uint64_t data_size = psize + 16;

    byte *packet = malloc(data_size);

    memcpy(packet, &pack->session_id, 8);
    memcpy(packet + 8, &pack->first_byte_num, 8);
    memcpy(packet + 16, pack->audio_data, psize);

    ssize_t sent_size = sendto(socket_fd, packet, data_size, flags,
                               (struct sockaddr *) dest_address,
                               address_length);

    ENSURE(sent_size == data_size);

    free(packet);
}

int main(int argc, char **argv) {
    sender_opts *opts = get_sender_opts(argc, argv);

    uint16_t port = opts->port;
    uint32_t psize = opts->psize;
    char *dest_addr = opts->dest_addr;

    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server_address = get_send_address(dest_addr, port);

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
