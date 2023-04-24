#ifndef _PACK_BUFFER_
#define _PACK_BUFFER_

#include <stdlib.h>
#include <stdint.h>
#include "common.h"

struct pack_buffer;

typedef struct pack_buffer pack_buffer;

/**
 * Initializes the pack buffer. Returns a pointer to struct.
 * @param bsize - size of pack buffer in bytes
 */
pack_buffer *pb_init(size_t bsize);

/**
 * Resets the pack buffer to initial state.
 * @param pb - pointer to pack buffer
 * @param psize - new audio_pack size
 * @param byte_zero - BYTE0 of this session
 */
void pb_reset(pack_buffer *pb, uint64_t psize, uint64_t byte_zero);

/**
 * Tries to insert the @p pack into the buffer. Throws a fatal error in case
 * @p psize differs from the psize of @p pb pack buffer.
 *
 * Finds all packs older than @p first_byte_num pack that could fit into the
 * buffer and are not present and prints their byte numbers to STDERR in
 * increasing order.
 * @param pb - pointer to pack buffer
 * @param first_byte_num - byte number identifying the pack
 * @param pack - pointer to pack's data
 * @param psize - size of @p pack in bytes
 */
void pb_push_back(pack_buffer *pb, uint64_t first_byte_num, const byte *pack,
                  uint64_t psize);

/**
 * Pops oldest pack from the pack buffer @p pb and stores it in @p item.
 * Blocks if pack buffer @p pb is not yet initialized, otherwise
 * non-blocking. If unable to take a pack from the buffer, it doesn't modify @p
 * item.
 * @param pb - pointer to pack buffer
 * @param item - result buffer
 * @returns psize of back buffer @p pb
 */
uint64_t pb_pop_front(pack_buffer *pb, void *item);

/**
 * Returns the number of packs in the pack buffer. Thread unsafe.
 * @param pb - pointer to pack buffer
 */
uint64_t pb_count(pack_buffer *pb);

#endif //_PACK_BUFFER_
