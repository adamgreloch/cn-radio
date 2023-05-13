#include "ctrl_protocol.h"
#include "opts.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define LOOKUP_STR "ZERO_SEVEN_COME_IN"
#define REPLY_STR "BOREWICZ_HERE"
#define REXMIT_STR "LOUDER_PLEASE"

int lookup_strlen = strlen(LOOKUP_STR);
int reply_strlen = strlen(REPLY_STR);
int rexmit_strlen = strlen(REXMIT_STR);

void write_lookup(char *buf) {
    sprintf(buf, "%s", LOOKUP_STR);
}

void write_reply(char *buf, char* mcast_addr_str, uint16_t port,
                               char* sender_name) {
    sprintf(buf, "%s %s %d %s", REPLY_STR, mcast_addr_str, port, sender_name);
}

void write_rexmit(char *buf, uint64_t *packs, uint64_t n_packs) {
    size_t wrote = sprintf(buf, "%s ", REXMIT_STR);

    for (uint64_t i = 0; i < n_packs - 1; i++)
        wrote += sprintf(buf + wrote, "%lu,", packs[i]);

    sprintf(buf + wrote, "%lu", packs[n_packs - 1]);
}

int what_message(char *buf) {
    if (strncmp(buf, LOOKUP_STR, lookup_strlen) == 0) return LOOKUP;
    if (strncmp(buf, REPLY_STR, reply_strlen) == 0) return REPLY;
    if (strncmp(buf, REXMIT_STR, rexmit_strlen) == 0) return REXMIT;
    return -1;
}

int parse_reply(char *msg, uint64_t msg_size, char* mcast_addr_str, uint16_t
    *port, char* sender_name) {
    char *prev_token;
    char *token;

    char* save_ptr;

    strtok_r(msg, " ", &save_ptr); // skip message specifier
    prev_token = strtok_r(NULL, " ", &save_ptr); // find pointer to
    // [mcast_addr] beginning
    token = strtok_r(NULL, " ", &save_ptr); // find pointer to [port] beginning

    memcpy(mcast_addr_str, prev_token, token - prev_token);

    // parse port and set token to sender name beginning
    *port = strtol(token, NULL, 10);

    token = strtok_r(NULL, "\0", &save_ptr); // get name
    if (msg_size + msg - token > MAX_NAME_LEN) return -1;

    memcpy(sender_name, token, msg_size + msg - token);

    return 0;
}

int parse_rexmit(char *msg, uint64_t *packs, uint64_t
*n_packs) {
    char *token;
    char* save_ptr;

    *n_packs = 0;

    strtok_r(msg, " ", &save_ptr); // skip message specifier
    token = strtok_r(NULL, ",", &save_ptr);
    while (token != NULL) {
        packs[(*n_packs)++] = strtoull(token, NULL, 10);
        token = strtok_r(NULL, ",", &save_ptr);
    }

    return 0;
}