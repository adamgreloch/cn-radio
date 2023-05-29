#ifndef _CTRL_PROTOCOL_
#define _CTRL_PROTOCOL_

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define LOOKUP 0
#define REPLY  1
#define REXMIT 2

// Define buffer size as 2^16 - a little more than maximum UDP data size
#define CTRL_BUF_SIZE 65536

/**
 * Writes a LOOKUP message to @p buf buffer. Assumes @p buf can fit the message.
 * @param buf - destination buffer
 * @returns written message size
 */
int write_lookup(char *buf);

/**
 * Writes a REPLY message to @p buf. Assumes @p buf can fit the message.
 * @param buf - destination buffer
 * @param mcast_addr_str - string representation of station's IPv4 address
 * @param port - station's port
 * @param name - station's name
 * @returns written message size
 */
int write_reply(char *buf, char *mcast_addr_str, uint16_t port,
                char *sender_name);

/**
 * Writes a REXMIT message to @p buf. Assumes @p buf can fit the message.
 * @param buf - destination buffer
 * @param packs - array of first_byte_nums of packs requested
 * @param n_packs - number of elements in @p packs
 * @returns written message size
 */
int write_rexmit(char *buf, uint64_t *packs, uint64_t n_packs);

/**
 * Tries to establish what message is in the @p buf.
 * @param buf
 * @returns @c LOOKUP, @c REPLY, @c REXMIT if matched; -1 otherwise
 */
int what_message(char *buf);

/**
 * Parses REPLY message stored in @p msg. Stores the retrieved information in
 * @p mcast_addr_str, @p port, @p sender_name pointers.
 * @param msg - message containing a valid REPLY message
 * @param msg_size - size of REPLY message
 * @param mcast_addr_str - pointer to string representation of station address
 * @param port - pointer to retrieved port
 * @param sender_name - pointer to retrieved name
 * @returns 0 if parsed successfully; -1 if message contained incorrect data
 */
int parse_reply(char *msg, uint64_t msg_size, char *mcast_addr_str, uint16_t
*port, char *sender_name);

/**
 * Parses REXMIT message stored in @p msg.
 * @param msg - message containing a valid REPLY message
 * @param packs - array of first_byte_nums of packs requested
 * @param n_packs - number of elements in @p packs
 * @returns 0 if parsed successfully
 */
int parse_rexmit(char *msg, uint64_t *packs, uint64_t
*n_packs);

#endif //_CTRL_PROTOCOL_
