#ifndef _COMMON_
#define _COMMON_

#include <netinet/in.h>
#include <stddef.h>
#include <netdb.h>
#include "err.h"

typedef uint8_t byte;

struct audio_pack {
    /** id of session from which the pack came from */
    uint64_t session_id;

    /** first byte number of the pack. is divisible by PSIZE */
    uint64_t first_byte_num;

    /** raw audio pack of exactly PSIZE bytes */
    byte *audio_data;
} __attribute__((__packed__));

inline static int bind_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    ENSURE(socket_fd > 0);
    // after socket() call; we should close(sock) on any execution path;

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(
            INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                     (socklen_t) sizeof(server_address)));

    return socket_fd;
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

inline static uint64_t htonll(uint64_t x) {
#if __BIG_ENDIAN__
    return x;
#else
    return ((uint64_t) htonl((x) & 0xFFFFFFFFLL) << 32) | htonl((x) >> 32);
#endif
}

inline static uint64_t ntohll(uint64_t x) {
#if __BIG_ENDIAN__
    return x;
#else
    return ((uint64_t) ntohl((x) & 0xFFFFFFFFLL) << 32) | ntohl((x) >> 32);
#endif
}

#endif //_COMMON_
