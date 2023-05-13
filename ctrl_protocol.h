#ifndef _CTRL_PROTOCOL_
#define _CTRL_PROTOCOL_

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define LOOKUP 0
#define REPLY  1
#define REXMIT 2

void write_lookup(char *buf);

void write_reply(char *buf, char* mcast_addr_str, uint16_t port,
                         char* sender_name);

void write_rexmit(char *buf, uint64_t *packs, uint64_t n_packs);

int what_message(char *buf);

int parse_reply(char *msg, uint64_t msg_size, char* mcast_addr_str, uint16_t
*port, char* sender_name);

int parse_rexmit(char *msg, uint64_t *packs, uint64_t
*n_packs);

#endif //_CTRL_PROTOCOL_
