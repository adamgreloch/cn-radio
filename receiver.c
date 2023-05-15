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

uint64_t curr_session_id;
uint64_t last_session_id = 0;

pack_buffer *audio_pack_buffer;
uint16_t ctrl_port;
uint16_t ui_port;
uint16_t port;
uint64_t bsize;
struct sockaddr_in listening_addr;
struct sockaddr_in discover_addr;

// IAC DO LINEMODE, IAC SB LINEMODE MODE 0 IAC SE, IAC WILL ECHO
char telnet_negotation[] = {255, 253, 34, 255, 250, 34, 1, 0, 255, 240, 255,
                            251, 1};

char *line_break =
        "------------------------------------------------------------------------\r\n";

#define DISCOVER_SLEEP 5

size_t receive_pack(int socket_fd, struct audio_pack **pack, byte *buffer,
                    uint64_t *psize) {
    size_t read_length;
    int flags = 0;
    errno = 0;

    memset(buffer, 0, bsize);

    struct sockaddr_in client_address;
    socklen_t address_length = (socklen_t) sizeof(client_address);

    read_length = recvfrom(socket_fd, buffer, bsize, flags, (struct sockaddr
    *) &client_address, &address_length);

    // FIXME radio stations may interfere on one multicast address

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

void *pack_receiver() {
    uint64_t psize;

    struct audio_pack *pack = malloc(sizeof(struct audio_pack));
    size_t read_length;

    int socket_fd = create_socket(port);

    enable_multicast(socket_fd, &listening_addr);

    byte *buffer = malloc(bsize);
    if (!buffer)
        fatal("malloc");

    while (true) {
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
        wrote_size = write_lookup(write_buffer);
        sent_size = sendto(ctrl_sock_fd, write_buffer, wrote_size,
                           flags, (struct sockaddr *)
                                   &discover_addr, discover_addr_len);
        ENSURE(sent_size == wrote_size);

//        fprintf(stderr, "sent lookup\n");

        sleep(DISCOVER_SLEEP);

        while ((recv_size = recvfrom(ctrl_sock_fd, write_buffer, CTRL_BUF_SIZE,
                                     MSG_DONTWAIT,
                                     (struct sockaddr *) &sender_addr,
                                     &sender_addr_len)) > 0) {
            if (what_message(write_buffer) == REPLY) {
                parse_reply(write_buffer, recv_size, mcast_addr_str,
                            &sender_port,
                            sender_name);
//                fprintf(stderr, "got reply from %s:%d '%s'\n", mcast_addr_str,
//                        sender_port, sender_name);
            }
        }
//        fprintf(stderr, "no more replies\n");
    }
}

#define INIT_PD_SIZE 16
#define QUEUE_LENGTH 8
#define TIMEOUT (-1)

// TODO set reasonable buf sizes
#define UI_BUF_SIZE 65536
#define NAV_BUF_SIZE 128

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

    char *ui_buffer = malloc(UI_BUF_SIZE);
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
                if (!accepted)
                    // Too many clients
                    CHECK_ERRNO(close(client_fd));
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
                    } else {
                        if (nav_buffer[0] == '\033' && nav_buffer[1] == '\133')
                            switch (nav_buffer[2]) {
                                case 'A':
                                    // arrow up
                                    fprintf(stderr, "arrow up\n");
                                    break;
                                case 'B':
                                    // arrow down
                                    fprintf(stderr, "arrow down\n");
                                    break;
                            }
                    }
                }
                if (pd[i].fd != -1 &&
                    (pd[i].revents & (POLLOUT))) {
                    ssize_t sent;
                    memset(ui_buffer, 0, UI_BUF_SIZE);
                    if (!negotiated[i]) {
                        for (size_t j = 0; j < sizeof(telnet_negotation); j++)
                            ui_buffer[j] = telnet_negotation[j];
                        sent = write(pd[i].fd, ui_buffer,
                                     sizeof(telnet_negotation));
                        ENSURE(sent == sizeof(telnet_negotation));
                        negotiated[i] = true;
                        fprintf(stderr, "telnet negotiation complete\n");
                    } else {
                        ssize_t ui_len = sprintf(ui_buffer, "\033[H\033[J");
                        ui_len += sprintf(ui_buffer + ui_len,
                                          "%s SIK Radio\r\n%s > Radio\r\n%s",
                                          line_break, line_break, line_break);
                        sent = write(pd[i].fd, ui_buffer, ui_len);
                        ENSURE(sent == ui_len);
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
    listening_addr = parse_host_and_port(opts->mcast_addr, opts->portstr);
    discover_addr = parse_host_and_port(opts->discover_addr,
                                        opts->ctrl_portstr);
    ui_port = opts->ui_port;

    audio_pack_buffer = pb_init(bsize);

    pthread_t receiver;
    pthread_t printer;
    pthread_t discoverer;
    pthread_t manager;

    CHECK_ERRNO(pthread_create(&receiver, NULL, pack_receiver, NULL));
    CHECK_ERRNO(pthread_create(&printer, NULL, pack_printer, NULL));
    CHECK_ERRNO(pthread_create(&discoverer, NULL, station_discoverer, NULL));
    CHECK_ERRNO(pthread_create(&manager, NULL, ui_manager, NULL));

    CHECK_ERRNO(pthread_join(receiver, NULL));
    CHECK_ERRNO(pthread_join(discoverer, NULL));
    CHECK_ERRNO(pthread_join(printer, NULL));
    CHECK_ERRNO(pthread_join(manager, NULL));

    return 0;
}
