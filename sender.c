#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include "err.h"
#include "common.h"
#include "ctrl_protocol.h"
#include "rexmit_queue.h"
#include "sender_utils.h"

static bool debug = false;

void *pack_sender(void *args) {
    sender_data *sd = args;
    uint64_t pack_num = 0;

    byte *read_bytes = (byte *) malloc(sd->psize);

    int mcast_send_sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in mcast_addr = get_send_address(sd->mcast_addr_str,
                                                     sd->port);
    enable_multicast(mcast_send_sock_fd, &mcast_addr);

    while (!feof(stdin)) {
        memset(read_bytes, 0, sd->psize);

        if (sd->psize == read_pack(stdin, sd->psize, read_bytes)) {
            struct audio_pack pack;

            pack.session_id = htonll(sd->session_id);
            pack.first_byte_num = htonll(pack_num * sd->psize);
            pack.audio_data = read_bytes;

            send_pack(mcast_send_sock_fd, &mcast_addr, &pack, sd);
            rq_add_pack(sd->rq, &pack);

            pack_num++;
        } else break;
    }

    CHECK_ERRNO(close(mcast_send_sock_fd));

    sd->finished = true;

    free(read_bytes);

    return 0;
}

void *ctrl_listener(void *args) {
    sender_data *sd = args;
    int ctrl_sock_fd = create_socket(sd->ctrl_port);

    char *buffer = malloc(CTRL_BUF_SIZE);
    uint64_t *packs = malloc(CTRL_BUF_SIZE);
    uint64_t n_packs;

    struct sockaddr_in receiver_addr;
    socklen_t address_length = (socklen_t) sizeof(receiver_addr);

    int flags = 0;
    errno = 0;

    int wrote_size;
    ssize_t sent_size;

    sockaddr_and_len receiver_sal;

    while (!sd->finished) {
        memset(buffer, 0, CTRL_BUF_SIZE);

        recvfrom(ctrl_sock_fd, buffer, CTRL_BUF_SIZE, flags,
                 (struct sockaddr *) &receiver_addr, &address_length);

        switch (what_message(buffer)) {
            case LOOKUP:
                memset(buffer, 0, CTRL_BUF_SIZE);
                wrote_size = write_reply(buffer, sd->mcast_addr_str, sd->port,
                                         sd->sender_name);
                sent_size = sendto(ctrl_sock_fd, buffer, wrote_size,
                                   flags, (struct sockaddr *)
                                           &receiver_addr,
                                   address_length);
                ENSURE(sent_size == wrote_size);
                break;
            case REXMIT:
                memset(packs, 0, CTRL_BUF_SIZE);
                parse_rexmit(buffer, packs, &n_packs);
                if (debug)
                    fprintf(stderr, "got rexmit!\n");

                receiver_sal.addr = receiver_addr;
                receiver_sal.addr_len = address_length;

                rq_bind_addr(sd->rq, packs, n_packs, &receiver_sal);
                break;
        }
    }

    CHECK_ERRNO(close(ctrl_sock_fd));

    return 0;
}

void *pack_retransmitter(void *args) {
    sender_data *sd = args;

    int send_sock_fd = open_socket();
    bind_socket(send_sock_fd, 0); // bind to any port

    byte *audio_data = malloc(sd->psize);
    uint64_t first_byte_num;

    struct audio_pack pack;

    sockaddr_and_len receiver_addr;

    while (!sd->finished) {
        usleep(sd->rtime_u);
        while (rq_pop_pack_for_addr(sd->rq, audio_data, &first_byte_num,
                                    &receiver_addr)) {
            pack.first_byte_num = first_byte_num;
            pack.session_id = sd->session_id;
            pack.audio_data = audio_data;
            send_pack(send_sock_fd, &receiver_addr.addr, &pack, sd);
            if (debug)
                fprintf(stderr, "retransmitted %lu\n", first_byte_num);
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    sender_data *sd = sd_init(argc, argv);

    pthread_t sender;
    pthread_t listener;
    pthread_t retransmitter;

    CHECK_ERRNO(pthread_create(&sender, NULL, pack_sender, sd));
    CHECK_ERRNO(pthread_create(&listener, NULL, ctrl_listener, sd));
    CHECK_ERRNO(pthread_create(&retransmitter, NULL, pack_retransmitter, sd));

    CHECK_ERRNO(pthread_join(sender, NULL));
    CHECK_ERRNO(pthread_join(listener, NULL));
    CHECK_ERRNO(pthread_join(retransmitter, NULL));

    sd_free(sd);

    return 0;
}
