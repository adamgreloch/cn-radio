#ifndef _COMMON_
#define _COMMON_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <stddef.h>
#include <netdb.h>
#include "err.h"

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

typedef uint8_t byte;

struct audio_pack {
    /** id of session from which the pack came from */
    uint64_t session_id;

    /** first byte number of the pack. is divisible by PSIZE */
    uint64_t first_byte_num;

    /** raw audio pack of exactly PSIZE bytes */
    byte *audio_data;
} __attribute__((__packed__));

inline static int open_socket() {
    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }

    return socket_fd;
}

inline static int create_socket(uint16_t port) {
    int socket_fd = open_socket();
    ENSURE(socket_fd > 0);
    // after socket() call; we should close(sock) on any execution path;

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(
            INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // make socket reusable
    int opt = 1;
    CHECK_ERRNO(
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)));

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                     (socklen_t) sizeof(server_address)));

    return socket_fd;
}

inline static int create_timeoutable_socket(uint16_t port) {
    int socket_fd = create_socket(port);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof
            (tv)));

    return socket_fd;
}

inline static void bind_socket(int socket_fd, uint16_t port) {
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(
            INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                     (socklen_t) sizeof(server_address)));
}


inline static void start_listening(int socket_fd, size_t queue_length) {
    CHECK_ERRNO(listen(socket_fd, queue_length));
}

inline static int
accept_connection(int socket_fd, struct sockaddr_in *client_address) {
    socklen_t client_address_length = (socklen_t) sizeof(*client_address);

    int client_fd = accept(socket_fd, (struct sockaddr *) client_address,
                           &client_address_length);
    if (client_fd < 0) {
        PRINT_ERRNO();
    }

    return client_fd;
}

inline static void
connect_socket(int socket_fd, const struct sockaddr_in *address) {
    CHECK_ERRNO(
            connect(socket_fd, (struct sockaddr *) address, sizeof(*address)));
}

inline static struct sockaddr_in get_send_address(char *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo *address_result;
    CHECK(getaddrinfo(host, NULL, &hints, &address_result));

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET; // IPv4
    send_address.sin_addr.s_addr =
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr; // IP address
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

inline static void enable_broadcast(int socket_fd) {
    int optval = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, (void *) &optval, sizeof
            (optval));
}

inline static void enable_multicast(int socket_fd, struct sockaddr_in
*mcast_addres) {
    struct ip_mreq ip_mreq;
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ip_mreq.imr_multiaddr = mcast_addres->sin_addr;

    ENSURE(IN_MULTICAST(ntohl(ip_mreq.imr_multiaddr.s_addr)));
    CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)
            &ip_mreq, sizeof(ip_mreq)));
}

inline static in_addr_t check_address(char *addr) {
    in_addr_t in_addr;
    int res = inet_pton(AF_INET, addr, &in_addr);
    if (res == -1)
        fatal("Failed to interpret address: %s", addr);
    else if (res == 0)
        fatal("Not a correct IPv4 address: %s", addr);
    return in_addr;
}

inline static struct sockaddr_in parse_host_and_port(const char *hostStr, const
char *portStr) {
    struct addrinfo *ai;
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = 0,
            .ai_protocol = 0, .ai_flags = 0};

    int gai_error;
    if ((gai_error = getaddrinfo(hostStr, portStr, &hints, &ai)) == -1) {
        fprintf(stderr, "getaddrinfo failed: %s", gai_strerror(gai_error));
    }

    struct sockaddr_in ret;
    ret = *(struct sockaddr_in *) (ai->ai_addr);
    freeaddrinfo(ai);

    return ret;
}

inline static bool is_number(char *str) {
    size_t i = 0;
    char c;
    while ((c = str[i]) != '\0')
        if (!isdigit(c))
            return false;
        else i++;
    return true;
}

inline static bool is_number_with_blanks(char *str) {
    size_t i = 0;
    int phase = 0;
    char c;
    while ((c = str[i]) != '\0') {
        if (phase == 0 && isdigit(c))
            phase = 1;
        else if (phase == 1 && isblank(c))
            phase = 2;
        else if ((phase == 2 && isdigit(c)) || (!isdigit(c) && !isblank(c)))
            return false;
        // else: if c is blank and phase 0, go ahead
        i++;
    }
    return phase != 0;
}

#endif //_COMMON_
