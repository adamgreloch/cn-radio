#ifndef _COMMON_
#define _COMMON_

#include <netinet/in.h>
#include <stddef.h>
#include <netdb.h>
#include "err.h"

#define DEFAULT_PSIZE 512
#define DEFAULT_PORT 28473
#define DEFAULT_BSIZE 65536

typedef uint8_t byte;

// TODO is of size PSIZE+16 bytes? it appears so
struct audio_pack {
    uint64_t session_id;
    uint64_t first_byte_num; // is divisible by PSIZE
    byte *audio_data; // has exactly PSIZE bytes
} __attribute__((__packed__));


int bind_socket(uint16_t port);

struct sockaddr_in get_send_address(char *host, uint16_t port);

uint64_t htonll(uint64_t x);

uint64_t ntohll(uint64_t x);

#endif //_COMMON_
