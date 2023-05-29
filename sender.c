#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include "err.h"
#include "common.h"
#include "ctrl_protocol.h"
#include "rexmit_queue.h"
#include "sender_utils.h"

static bool debug = false;

static void *pack_sender(void *args) {
    sender_data *sd = args;
    uint64_t pack_num = 0;

    byte *read_bytes = (byte *) malloc(sd->psize);

    while (!feof(stdin)) {
        memset(read_bytes, 0, sd->psize);

        if (sd->psize == read_pack(stdin, sd->psize, read_bytes)) {
            struct audio_pack pack;

            pack.session_id = htobe64(sd->session_id);
            pack.first_byte_num = be64toh(pack_num * sd->psize);
            pack.audio_data = read_bytes;

            send_pack(sd->mcast_send_sock_fd, &sd->mcast_addr, &pack, sd);
            rq_add_pack(sd->rq, &pack);

            pack_num++;
        } else break;
    }

    mark_finished(sd);

    free(read_bytes);

    return 0;
}

static void *ctrl_listener(void *args) {
    sender_data *sd = args;
    int ctrl_sock_fd = create_timeoutable_socket(sd->ctrl_port);

    char *buffer = malloc(CTRL_BUF_SIZE);
    uint64_t *packs = malloc(CTRL_BUF_SIZE);
    uint64_t n_packs;

    struct sockaddr_in receiver_addr;
    socklen_t address_length = (socklen_t) sizeof(receiver_addr);

    int flags = 0;

    int wrote_size;
    ssize_t sent_size;

    while (!is_finished(sd)) {
        memset(buffer, 0, CTRL_BUF_SIZE);

        recvfrom(ctrl_sock_fd, buffer, CTRL_BUF_SIZE, flags,
                 (struct sockaddr *) &receiver_addr, &address_length);

        switch (what_message(buffer)) {
            case LOOKUP:
                memset(buffer, 0, CTRL_BUF_SIZE);
                wrote_size = write_reply(buffer, sd->mcast_addr_str, sd->port,
                                         sd->sender_name);
                errno = 0;
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
                rq_add_requests(sd->rq, packs, n_packs);
                break;
        }
    }

    CHECK_ERRNO(close(ctrl_sock_fd));
    free(buffer);
    free(packs);

    return 0;
}

static void *pack_retransmitter(void *args) {
    sender_data *sd = args;

    int send_sock_fd = open_socket();
    bind_socket(send_sock_fd, 0); // bind to any port

    byte *audio_data = malloc(sd->psize);

    struct audio_pack pack;

    uint64_t *requested_nums = NULL;
    uint64_t arr_size = 0;
    uint64_t n_packs;

    while (!is_finished(sd)) {
        if ((n_packs = rq_get_requests(sd->rq, &requested_nums,
                                       &arr_size)) > 0) {
            fprintf(stderr, "%lu to retransmit\n", n_packs);
            for (uint64_t i = 0; i < n_packs; i++)
                if (rq_get_pack(sd->rq, audio_data, requested_nums[i])) {
                    pack.first_byte_num = htobe64(requested_nums[i]);
                    pack.session_id = htobe64(sd->session_id);
                    pack.audio_data = audio_data;
                    send_pack(sd->mcast_send_sock_fd, &sd->mcast_addr, &pack,
                              sd);
                    if (debug)
                        fprintf(stderr, "retransmitted %lu\n",
                                requested_nums[i]);
                }
        }
        usleep(sd->rtime_u);
    }

    free(audio_data);

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
