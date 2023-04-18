#ifndef _COMMON_
#define _COMMON_

#include <netinet/in.h>
#include <stddef.h>
#include <netdb.h>
#include "err.h"

typedef uint8_t byte;

struct audio_pack {
    /** id of session frm which the pack came from */
    uint64_t session_id;

    /** first byte number of the pack. is divisible by PSIZE */
    uint64_t first_byte_num;

    /** raw audio pack of exactly PSIZE bytes */
    byte *audio_data;
} __attribute__((__packed__));

int bind_socket(uint16_t port);

struct sockaddr_in get_send_address(char *host, uint16_t port);

uint64_t htonll(uint64_t x);

uint64_t ntohll(uint64_t x);

#endif //_COMMON_
