#include <netinet/in.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "err.h"
#include "common.h"
#include "opts.h"
#include "ctrl_protocol.h"
//#include "rexmit_queue.h"

char *sender_name;
char *mcast_addr_str;
int mcast_send_sock_fd;
struct sockaddr_in mcast_addr;

uint16_t port;
uint16_t ctrl_port;
uint32_t psize;
uint64_t rtime_u;

bool finished = false;

char *send_buffer;

//rexmit_queue *rq;

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

            send_pack(mcast_send_sock_fd, &mcast_addr, &pack, psize);

            pack_num++;
        } else break;
    }

    finished = true;

    free(read_bytes);

    return 0;
}

void *ctrl_listener() {
    int ctrl_sock_fd = create_socket(ctrl_port);

    char *buffer = malloc(CTRL_BUF_SIZE);
    uint64_t *packs = malloc(CTRL_BUF_SIZE);
    uint64_t n_packs;

    struct sockaddr_in receiver_addr;
    socklen_t address_length = (socklen_t) sizeof(receiver_addr);

    int flags = 0;
    errno = 0;

    int wrote_size;
    ssize_t sent_size;

    while (!finished) {
        memset(buffer, 0, CTRL_BUF_SIZE);

        recvfrom(ctrl_sock_fd, buffer, CTRL_BUF_SIZE, flags,
                 (struct sockaddr *) &receiver_addr, &address_length);

        switch (what_message(buffer)) {
            case LOOKUP:
                memset(buffer, 0, CTRL_BUF_SIZE);
                wrote_size = write_reply(buffer, mcast_addr_str, port,
                                         sender_name);
                sent_size = sendto(ctrl_sock_fd, buffer, wrote_size,
                                   flags, (struct sockaddr *)
                                           &receiver_addr,
                                   address_length);
                ENSURE(sent_size == wrote_size);
                break;
            case REXMIT:
                memset(packs, 0, CTRL_BUF_SIZE);
                parse_rexmit(buffer, packs, &n_packs);
                fprintf(stderr, "got rexmit!\n");
//                rs_add(rq, packs, n_packs);
                break;
        }
    }

    CHECK_ERRNO(close(ctrl_sock_fd));

    return 0;
}

void *pack_retransmitter() {
    while (!finished) {
        usleep(rtime_u);
        // TODO retransmitting
        //  * write FIFO
        //  * write rs
    }
    return 0;
}

int main(int argc, char **argv) {
    sender_opts *opts = get_sender_opts(argc, argv);

    port = opts->port;
    ctrl_port = opts->ctrl_port;
    psize = opts->psize;
    sender_name = opts->sender_name;
    rtime_u = opts->rtime * 1000; // microseconds

    send_buffer = calloc(psize + 16, 1);
    if (!send_buffer)
        fatal("calloc");

    check_address(opts->mcast_addr_str);
    mcast_addr_str = opts->mcast_addr_str;

    mcast_send_sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
    mcast_addr = get_send_address(mcast_addr_str, port);
    enable_multicast(mcast_send_sock_fd, &mcast_addr);

    pthread_t sender;
    pthread_t listener;
    pthread_t retransmitter;

    CHECK_ERRNO(pthread_create(&sender, NULL, pack_sender, NULL));
    CHECK_ERRNO(pthread_create(&listener, NULL, ctrl_listener, NULL));
    CHECK_ERRNO(pthread_create(&retransmitter, NULL, pack_retransmitter, NULL));

    CHECK_ERRNO(pthread_join(sender, NULL));
    CHECK_ERRNO(pthread_join(listener, NULL));
    CHECK_ERRNO(pthread_join(retransmitter, NULL));

    CHECK_ERRNO(close(mcast_send_sock_fd));

    free(send_buffer);
    free(opts);

    return 0;
}
