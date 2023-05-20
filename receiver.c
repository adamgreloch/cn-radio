#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include "common.h"
#include "err.h"
#include "pack_buffer.h"
#include "opts.h"
#include "ctrl_protocol.h"
#include "receiver_ui.h"

uint64_t curr_session_id;
uint64_t last_session_id = 0;

pack_buffer *audio_pack_buffer;
uint16_t ctrl_port;
uint16_t ui_port;
uint16_t port;
uint64_t bsize;
uint64_t rtime_u;
struct sockaddr_in discover_addr;

stations *st;

station *curr_station;

#define DISCOVER_SLEEP 5
#define INACTIVITY_THRESH 20

struct sockaddr_in client_address;
socklen_t client_address_len = (socklen_t) sizeof(client_address);

size_t receive_pack(int socket_fd, struct audio_pack **pack, byte *buffer,
                    uint64_t *psize) {
    ssize_t read_length;
    int flags = 0;
    errno = 0;

    memset(buffer, 0, bsize);

    read_length = recvfrom(socket_fd, buffer, bsize, flags, (struct sockaddr
    *) &client_address, &client_address_len);

    if (read_length < 0) {
        remove_current_for_inactivity(st);
        return 0;
    }

    *psize = read_length - 16;

    memcpy(&(*pack)->session_id, buffer, 8);
    memcpy(&(*pack)->first_byte_num, buffer + 8, 8);
    (*pack)->audio_data = buffer + 16;

    curr_session_id = ntohll((*pack)->session_id);

    if (curr_session_id > last_session_id)
        pb_reset(audio_pack_buffer, *psize, ntohll((*pack)->first_byte_num));

    if (curr_session_id < last_session_id)
        return 0;

    last_session_id = curr_session_id;

    return read_length;
}

int create_recv_socket(uint16_t port, struct sockaddr_in *mcast_addr) {
    int socket_fd = create_socket(port);

    struct timeval tv;
    tv.tv_sec = INACTIVITY_THRESH;
    tv.tv_usec = 0;
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof
            (tv)));

    enable_multicast(socket_fd, mcast_addr);

    return socket_fd;
}

void *pack_receiver() {
    uint64_t psize;

    struct audio_pack *pack = malloc(sizeof(struct audio_pack));
    size_t read_length;

    int socket_fd = -1;

    byte *buffer = malloc(bsize);
    if (!buffer)
        fatal("malloc");

    struct sockaddr_in station_addr;

    // TODO rewrite to wait for station with given name
    wait_until_any_station_found(st);

    while (true) {
        if (switch_if_changed(st, &curr_station)) {
            if (socket_fd > 0)
                CHECK_ERRNO(close(socket_fd));

            inet_aton(curr_station->mcast_addr, &station_addr.sin_addr);
            socket_fd = create_recv_socket(curr_station->port, &station_addr);
            last_session_id = 0;
        }

        read_length = receive_pack(socket_fd, &pack, buffer, &psize);

        if (read_length > 0)
            pb_push_back(audio_pack_buffer, ntohll(pack->first_byte_num),
                         pack->audio_data, psize);
    }
}

void *pack_printer() {
    byte *write_buffer = malloc(bsize);
    if (!write_buffer)
        fatal("malloc");

    uint64_t psize;

    while (true) {
        memset(write_buffer, 0, bsize);
        psize = pb_pop_front(audio_pack_buffer, write_buffer);
        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }
}

#define UDP_IPV4_DATASIZE 65507

#define max(a, b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

#define min(a, b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})

void *missing_reporter() {
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
    errno = 0;

    uint64_t *missing_buf = NULL;
    uint64_t buf_size = 0;

    wait_until_any_station_found(st);

    while (true) {
        usleep(rtime_u);
        pb_find_missing(audio_pack_buffer, &n_packs_total, &missing_buf,
                        &buf_size);

        if (n_packs_total > 0)
            while (n_packs_total > n_packs_sent) {
                n_packs_to_send = min(n_packs_total - n_packs_sent,
                                      UDP_IPV4_DATASIZE / sizeof(uint64_t));

                wrote_size = write_rexmit(write_buffer,
                                          missing_buf + n_packs_sent,
                                          n_packs_to_send);
                n_packs_sent += n_packs_to_send;

                client_address.sin_port = htons(ctrl_port);

                sent_size = sendto(send_sock_fd, write_buffer, wrote_size,
                                   flags, (struct sockaddr *)
                                           &client_address, client_address_len);
                ENSURE(sent_size == wrote_size);
            }
        n_packs_sent = 0;
    }
}

void *station_discoverer() {
    int ctrl_sock_fd = create_socket(ctrl_port);
    enable_broadcast(ctrl_sock_fd);

    char *write_buffer = malloc(CTRL_BUF_SIZE);
    if (!write_buffer)
        fatal("malloc");

    int wrote_size;
    ssize_t sent_size;
    ssize_t recv_size;
    int flags = 0;
    errno = 0;

    socklen_t discover_addr_len = (socklen_t) sizeof(discover_addr);

    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = (socklen_t) sizeof(sender_addr);

    char mcast_addr_str[20];
    uint16_t sender_port;
    char sender_name[64 + 1];

    while (true) {
        memset(write_buffer, 0, CTRL_BUF_SIZE);

        wrote_size = write_lookup(write_buffer);
        sent_size = sendto(ctrl_sock_fd, write_buffer, wrote_size,
                           flags, (struct sockaddr *)
                                   &discover_addr, discover_addr_len);
        ENSURE(sent_size == wrote_size);

        sleep(DISCOVER_SLEEP);

        memset(write_buffer, 0, CTRL_BUF_SIZE);

        while ((recv_size = recvfrom(ctrl_sock_fd, write_buffer, CTRL_BUF_SIZE,
                                     MSG_DONTWAIT,
                                     (struct sockaddr *) &sender_addr,
                                     &sender_addr_len)) > 0) {
            if (what_message(write_buffer) == REPLY) {
                parse_reply(write_buffer, recv_size, mcast_addr_str,
                            &sender_port,
                            sender_name);
                update_station(st, mcast_addr_str, sender_port, sender_name);
            }

            memset(mcast_addr_str, 0, sizeof(mcast_addr_str));
            memset(sender_name, 0, sizeof(sender_name));
            memset(write_buffer, 0, CTRL_BUF_SIZE);
        }

        delete_inactive_stations(st, INACTIVITY_THRESH);
    }
}

#define INIT_PD_SIZE 16
#define QUEUE_LENGTH 8
#define TIMEOUT (-1)

// TODO set reasonable buf sizes
#define UI_BUF_SIZE 512
#define NAV_BUF_SIZE 128

// IAC DO LINEMODE, IAC SB LINEMODE MODE 0 IAC SE, IAC WILL ECHO
char telnet_negotation[] = {255, 253, 34, 255, 250, 34, 1, 0, 255, 240, 255,
                            251, 1};

void negotiate_telnet(int fd, char *buf) {
    for (size_t i = 0; i < sizeof(telnet_negotation); i++)
        buf[i] = telnet_negotation[i];
    ssize_t sent = write(fd, buf, sizeof(telnet_negotation));
    ENSURE(sent == sizeof(telnet_negotation));
}

void handle_input(char *nav_buffer) {
    if (nav_buffer[0] == '\033' && nav_buffer[1] == '\133')
        switch (nav_buffer[2]) {
            case 'A': // Received arrow up
                select_station_up(st);
                break;
            case 'B': // Received arrow down
                select_station_down(st);
                break;
        }
}

void *ui_manager() {
    int pd_size = INIT_PD_SIZE;

    struct pollfd *pd = calloc(pd_size, sizeof(struct pollfd));
    bool *negotiated = calloc(pd_size, sizeof(bool));

    // initialize client socket table, pd[0] is a central socket
    for (int i = 0; i < pd_size; i++) {
        pd[i].fd = -1;
        pd[i].events = POLLIN | POLLOUT;
        pd[i].revents = 0;
    }

    pd[0].fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (pd[0].fd < 0) {
        PRINT_ERRNO();
    }

    uint64_t ui_buffer_size = UI_BUF_SIZE;
    char *ui_buffer = malloc(sizeof(UI_BUF_SIZE) * ui_buffer_size);
    if (!ui_buffer)
        fatal("malloc");

    char *nav_buffer = malloc(NAV_BUF_SIZE);
    if (!nav_buffer)
        fatal("malloc");

    bind_socket(pd[0].fd, ui_port);

    start_listening(pd[0].fd, QUEUE_LENGTH);

    while (true) {
        for (int i = 0; i < pd_size; i++) {
            pd[i].revents = 0;
        }

        sleep(1);
        int poll_status = poll(pd, pd_size, TIMEOUT);
        if (poll_status == -1)
            PRINT_ERRNO();
        else if (poll_status > 0) {
            if (pd[0].revents & POLLIN) {
                // Accept new connection
                int client_fd = accept_connection(pd[0].fd, NULL);

                bool accepted = false;
                for (int i = 1; i < pd_size; i++) {
                    if (pd[i].fd == -1) {
                        pd[i].fd = client_fd;
                        accepted = true;
                        break;
                    }
                }

                if (!accepted) {
                    // pd table is too small. Resize it
                    pd_size *= 2;
                    pd = realloc(pd, pd_size);
                    if (!pd)
                        fatal("realloc");
                    pd[pd_size].fd = client_fd;
                }
            }

            for (int i = 1; i < pd_size; i++) {
                if (pd[i].fd != -1 &&
                    (pd[i].revents & (POLLIN | POLLERR))) {
                    ssize_t received_bytes = read(pd[i].fd, nav_buffer,
                                                  NAV_BUF_SIZE);
                    if (received_bytes < 0) {
                        // Error when reading message from connection
                        pd[i].fd = -1;
                    } else if (received_bytes == 0) {
                        // Client has closed connection
                        CHECK_ERRNO(close(pd[i].fd));
                        pd[i].fd = -1;
                    } else
                        handle_input(nav_buffer);
                }
                if (pd[i].fd != -1 &&
                    (pd[i].revents & (POLLOUT))) {
                    ssize_t sent;
                    memset(ui_buffer, 0, UI_BUF_SIZE);
                    if (!negotiated[i]) {
                        negotiate_telnet(pd[i].fd, ui_buffer);
                        negotiated[i] = true;
                    } else {
                        size_t ui_len;
                        print_ui(&ui_buffer, &ui_buffer_size, &ui_len, st);
                        sent = write(pd[i].fd, ui_buffer, ui_len);
                        ENSURE(sent == (ssize_t) ui_len);
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    receiver_opts *opts = get_receiver_opts(argc, argv);

    port = opts->port;
    bsize = opts->bsize;
    ctrl_port = opts->ctrl_port;
    discover_addr = parse_host_and_port(opts->discover_addr,
                                        opts->ctrl_portstr);
    rtime_u = opts->rtime * 1000; // microseconds

    ui_port = opts->ui_port;

    audio_pack_buffer = pb_init(bsize);

    st = init_stations();

    curr_station = malloc(sizeof(station));

    pthread_t receiver;
    pthread_t printer;
    pthread_t discoverer;
    pthread_t manager;
    pthread_t reporter;

    CHECK_ERRNO(pthread_create(&receiver, NULL, pack_receiver, NULL));
    CHECK_ERRNO(pthread_create(&printer, NULL, pack_printer, NULL));
    CHECK_ERRNO(pthread_create(&discoverer, NULL, station_discoverer, NULL));
    CHECK_ERRNO(pthread_create(&manager, NULL, ui_manager, NULL));
    CHECK_ERRNO(pthread_create(&reporter, NULL, missing_reporter, NULL));

    CHECK_ERRNO(pthread_join(receiver, NULL));
    CHECK_ERRNO(pthread_join(discoverer, NULL));
    CHECK_ERRNO(pthread_join(printer, NULL));
    CHECK_ERRNO(pthread_join(manager, NULL));
    CHECK_ERRNO(pthread_join(reporter, NULL));

    return 0;
}
