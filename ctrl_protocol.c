#include "ctrl_protocol.h"
#include "opts.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define LOOKUP_STR "ZERO_SEVEN_COME_IN\n"
#define REPLY_STR "BOREWICZ_HERE"
#define REXMIT_STR "LOUDER_PLEASE"

int lookup_strlen = strlen(LOOKUP_STR);
int reply_strlen = strlen(REPLY_STR);
int rexmit_strlen = strlen(REXMIT_STR);

int write_lookup(char *buf) {
    return sprintf(buf, "%s", LOOKUP_STR);
}

int write_reply(char *buf, char *mcast_addr_str, uint16_t port,
                char *sender_name) {
    return sprintf(buf, "%s %s %d %s\n", REPLY_STR, mcast_addr_str, port,
                   sender_name);
}

int write_rexmit(char *buf, uint64_t *packs, uint64_t n_packs) {
    if (n_packs == 0) return 0;
    size_t wrote = sprintf(buf, "%s ", REXMIT_STR);

    for (uint64_t i = 0; i < n_packs - 1; i++)
        wrote += sprintf(buf + wrote, "%lu,", packs[i]);

    wrote += sprintf(buf + wrote, "%lu\n", packs[n_packs - 1]);
    return wrote;
}

int what_message(char *buf) {
    if (strncmp(buf, LOOKUP_STR, lookup_strlen) == 0) return LOOKUP;
    if (strncmp(buf, REPLY_STR, reply_strlen) == 0) return REPLY;
    if (strncmp(buf, REXMIT_STR, rexmit_strlen) == 0) return REXMIT;
    return -1;
}

int parse_reply(char *msg, uint64_t msg_size, char *mcast_addr_str, uint16_t
*port, char *sender_name) {
    char *prev_token;
    char *token;

    char *save_ptr;

    strtok_r(msg, " ", &save_ptr); // skip message specifier
    prev_token = strtok_r(NULL, " ", &save_ptr); // find pointer to
    // [mcast_addr] beginning
    token = strtok_r(NULL, " ", &save_ptr); // find pointer to [port] beginning

    in_addr_t in_addr;
    if (inet_pton(AF_INET, prev_token, &in_addr) <= 0
        || !IN_MULTICAST(ntohl(in_addr)))
        return -1;

    memcpy(mcast_addr_str, prev_token, token - prev_token);

    // parse port and set token to sender name beginning
    uint64_t read_port;
    errno = 0;
    read_port = strtoll(token, NULL, 10);
    if (read_port == 0 || read_port > 65535 || errno != 0) return -1;
    *port = read_port;

    token = strtok_r(NULL, "\n", &save_ptr); // get name
    if (msg_size + msg - token > MAX_NAME_LEN) return -1;

    memcpy(sender_name, token, msg_size + msg - token);

    return 0;
}

int parse_rexmit(char *msg, uint64_t *packs, uint64_t *n_packs) {
    char *token;
    char *save_ptr;

    fprintf(stderr, "REXMIT: '%s'", msg);

    *n_packs = 0;

    uint64_t byte_num;

    strtok_r(msg, " ", &save_ptr); // skip message specifier
    token = strtok_r(NULL, ",", &save_ptr);
    while (token != NULL) {
        if (is_number_with_blanks(token)) {
            errno = 0;
            byte_num = strtoull(token, NULL, 10);
            if (errno == 0)
                packs[(*n_packs)++] = byte_num;
            fprintf(stderr, "strtoull got: %s -> %lu\n", token, byte_num);
        }
        token = strtok_r(NULL, ",", &save_ptr);
    }

    return 0;
}