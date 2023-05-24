#ifndef _SENDER_UTILS_
#define _SENDER_UTILS_

#include <stdint.h>
#include <unistd.h>
#include <netinet/in.h>
#include <time.h>
#include "err.h"
#include "rexmit_queue.h"
#include "opts.h"

struct sender_data {
    char *sender_name;
    char *mcast_addr_str;

    uint16_t port;
    uint16_t ctrl_port;
    uint64_t psize;
    uint64_t fsize;
    uint64_t rtime_u;
    uint64_t session_id;

    bool finished;

    char *send_buffer;

    rexmit_queue *rq;

    sender_opts *opts;
};

typedef struct sender_data sender_data;

inline static sender_data *sd_init(int argc, char **argv) {
    sender_data *sd = malloc(sizeof(sender_data));
    sender_opts *opts = get_sender_opts(argc, argv);

    sd->port = opts->port;
    sd->ctrl_port = opts->ctrl_port;
    sd->psize = opts->psize;
    sd->sender_name = opts->sender_name;
    sd->rtime_u = opts->rtime * 1000; // microseconds
    sd->fsize = opts->fsize;
    sd->session_id = time(NULL);
    sd->finished = false;

    sd->send_buffer = calloc(sd->psize + 16, 1);
    if (!sd->send_buffer)
        fatal("calloc");

    check_address(opts->mcast_addr_str);

    sd->mcast_addr_str = opts->mcast_addr_str;
    sd->rq = rq_init(sd->psize, sd->fsize);

    sd->opts = opts;

    return sd;
}

inline static void sd_free(sender_data *sd) {
    free(sd->send_buffer);
    free(sd->opts);
    free(sd);
}


inline static size_t read_pack(FILE *stream, uint64_t pack_size, byte *data) {
    return fread(data, sizeof(byte), pack_size, stream);
}

inline static void send_pack(int socket_fd, const struct sockaddr_in
        *dest_address,
               const struct audio_pack *pack, sender_data *sd) {
    socklen_t address_length = (socklen_t) sizeof(*dest_address);
    int flags = 0;

    ssize_t data_size = sd->psize + 16;

    memset(sd->send_buffer, 0, data_size);

    memcpy(sd->send_buffer, &pack->session_id, 8);
    memcpy(sd->send_buffer + 8, &pack->first_byte_num, 8);
    memcpy(sd->send_buffer + 16, pack->audio_data, sd->psize);

    ssize_t sent_size = sendto(socket_fd, sd->send_buffer, data_size, flags,
                               (struct sockaddr *) dest_address,
                               address_length);

    ENSURE(sent_size == data_size);
}


#endif //_SENDER_UTILS_