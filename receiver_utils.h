#ifndef _RECEIVER_UTILS_
#define _RECEIVER_UTILS_

#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "err.h"
#include "common.h"
#include "pack_buffer.h"
#include "receiver_ui.h"
#include "opts.h"
#include "receiver_utils.h"

struct receiver_data {
    uint64_t curr_session_id;
    uint64_t last_session_id;

    pack_buffer *pb;
    uint16_t ctrl_port;
    uint16_t ui_port;
    uint64_t bsize;
    uint64_t rtime_u;
    struct sockaddr_in discover_addr;

    char *prioritized_name;

    stations *st;

    struct sockaddr_in client_address;
    socklen_t client_address_len;

    pthread_mutex_t mutex;
};

typedef struct receiver_data receiver_data;

inline static receiver_data *rd_init(int argc, char **argv) {
    receiver_opts *opts = get_receiver_opts(argc, argv);
    receiver_data *rd = malloc(sizeof(receiver_data));

    rd->bsize = opts->bsize;
    rd->ctrl_port = opts->ctrl_port;
    rd->discover_addr = parse_host_and_port(opts->discover_addr,
                                            opts->ctrl_portstr);
    rd->rtime_u = opts->rtime * 1000; // microseconds
    rd->ui_port = opts->ui_port;
    rd->pb = pb_init(rd->bsize);
    rd->st = init_stations();

    rd->prioritized_name = opts->sender_name;

    rd->client_address_len = (socklen_t) sizeof(rd->client_address);
    rd->last_session_id = 0;

    CHECK_ERRNO(pthread_mutex_init(&rd->mutex, NULL));

    return rd;
}

inline static void negotiate_telnet(int fd, char *buf) {
    // IAC DO LINEMODE, IAC SB LINEMODE MODE 0 IAC SE, IAC WILL ECHO
    char telnet_negotation[] = {255, 253, 34, 255, 250, 34, 1, 0, 255, 240,
                                255, 251, 1};
    for (size_t i = 0; i < sizeof(telnet_negotation); i++)
        buf[i] = telnet_negotation[i];
    ssize_t sent = write(fd, buf, sizeof(telnet_negotation));
    ENSURE(sent == sizeof(telnet_negotation));
}

inline static void handle_input(char *nav_buffer, stations *st) {
    if (nav_buffer[0] == '\033' && nav_buffer[1] == '\133')
        switch (nav_buffer[2]) {
            case 'A': // Received arrow up
                st_select_station_up(st);
                break;
            case 'B': // Received arrow down
                st_select_station_down(st);
                break;
        }
}

inline static int create_recv_socket(uint16_t port, struct sockaddr_in
*mcast_addr) {
    int socket_fd = create_socket(port);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof
            (tv)));
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof
            (int)));

    enable_multicast(socket_fd, mcast_addr);

    return socket_fd;
}

inline static size_t receive_pack(int socket_fd, struct audio_pack **pack, byte
*buffer,
                                  uint64_t *psize, receiver_data *rd) {
    ssize_t read_length;
    int flags = 0;
    errno = 0;

    memset(buffer, 0, rd->bsize);

    CHECK_ERRNO(pthread_mutex_lock(&rd->mutex));
    read_length = recvfrom(socket_fd, buffer, rd->bsize, flags, (struct sockaddr
    *) &rd->client_address, &rd->client_address_len);
    CHECK_ERRNO(pthread_mutex_unlock(&rd->mutex));

    if (read_length < 0)
        return 0;

    *psize = read_length - 16;

    memcpy(&(*pack)->session_id, buffer, 8);
    memcpy(&(*pack)->first_byte_num, buffer + 8, 8);
    (*pack)->audio_data = buffer + 16;

    rd->curr_session_id = ntohll((*pack)->session_id);

    if (rd->curr_session_id > rd->last_session_id)
        pb_reset(rd->pb, *psize, ntohll((*pack)->first_byte_num));

    if (rd->curr_session_id < rd->last_session_id)
        return 0;

    rd->last_session_id = rd->curr_session_id;

    return read_length;
}


#endif //_RECEIVER_UTILS_
