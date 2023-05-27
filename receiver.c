#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"
#include "err.h"
#include "pack_buffer.h"
#include "ctrl_protocol.h"
#include "receiver_ui.h"
#include "receiver_utils.h"

static void *pack_receiver(void *args) {
    receiver_data *rd = args;

    uint64_t psize;

    struct audio_pack *pack = malloc(sizeof(struct audio_pack));
    size_t read_length;

    int socket_fd = -1;

    byte *buffer = malloc(rd->bsize);
    if (!buffer)
        fatal("malloc");

    struct sockaddr_in station_addr;

    station curr_station;


    while (true) {
        if (st_switch_if_changed(rd->st, &curr_station)) {
            if (socket_fd > 0)
                CHECK_ERRNO(close(socket_fd));

            inet_aton(curr_station.mcast_addr, &station_addr.sin_addr);
            socket_fd = create_timeoutable_socket(curr_station.port);
            enable_multicast(socket_fd, &station_addr);
            rd->last_session_id = 0;
        }

        read_length = receive_pack(socket_fd, &pack, buffer, &psize, rd);

        if (read_length > 0)
            pb_push_back(rd->pb, be64toh(pack->first_byte_num),
                         pack->audio_data, psize);
    }

    return 0;
}

static void *pack_printer(void *args) {
    receiver_data *rd = args;

    byte *write_buffer = malloc(rd->bsize);
    if (!write_buffer)
        fatal("malloc");

    uint64_t psize;

    while (true) {
        memset(write_buffer, 0, rd->bsize);
        psize = pb_pop_front(rd->pb, write_buffer);
        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }

    return 0;
}

static void *missing_reporter(void *args) {
    receiver_data *rd = args;

    int send_sock_fd = open_socket();
    bind_socket(send_sock_fd, 0); // bind to any port

    char *write_buffer = malloc(UDP_IPV4_DATASIZE);
    if (!write_buffer)
        fatal("malloc");

    uint64_t n_packs_total = 0;
    uint64_t n_packs_to_send;
    uint64_t n_packs_sent = 0;

    int wrote_size;
    ssize_t sent_size;
    int flags = 0;

    uint64_t *missing_buf = NULL;
    uint64_t buf_size = 0;

    st_wait_until_station_found(rd->st);

    while (true) {
        pb_find_missing(rd->pb, &n_packs_total, &missing_buf,
                        &buf_size);

        if (n_packs_total > 0)
            while (n_packs_total > n_packs_sent) {
                n_packs_to_send = min(n_packs_total - n_packs_sent,
                                      UDP_IPV4_DATASIZE / sizeof(uint64_t));

                wrote_size = write_rexmit(write_buffer,
                                          missing_buf + n_packs_sent,
                                          n_packs_to_send);
                n_packs_sent += n_packs_to_send;

                CHECK_ERRNO(pthread_mutex_lock(&rd->mutex));
                rd->client_address.sin_port = htons(rd->ctrl_port);

                errno = 0;
                sent_size = sendto(send_sock_fd, write_buffer, wrote_size,
                                   flags, (struct sockaddr *)
                                           &rd->client_address,
                                   rd->client_address_len);
                CHECK_ERRNO(pthread_mutex_unlock(&rd->mutex));
                ENSURE(sent_size == wrote_size);
            }
        n_packs_sent = 0;
        usleep(rd->rtime_u);
    }

    return 0;
}

int main(int argc, char **argv) {
    receiver_data *rd = rd_init(argc, argv);

    pthread_t receiver, printer, discoverer, manager, reporter;

    CHECK_ERRNO(pthread_create(&receiver, NULL, pack_receiver, rd));
    CHECK_ERRNO(pthread_create(&printer, NULL, pack_printer, rd));
    CHECK_ERRNO(pthread_create(&discoverer, NULL, station_discoverer, rd));
    CHECK_ERRNO(pthread_create(&manager, NULL, ui_manager, rd));
    CHECK_ERRNO(pthread_create(&reporter, NULL, missing_reporter, rd));

    CHECK_ERRNO(pthread_join(receiver, NULL));
    CHECK_ERRNO(pthread_join(discoverer, NULL));
    CHECK_ERRNO(pthread_join(printer, NULL));
    CHECK_ERRNO(pthread_join(manager, NULL));
    CHECK_ERRNO(pthread_join(reporter, NULL));

    return 0;
}
