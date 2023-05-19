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
pack_buffer *pb_init(uint64_t bsize);

/**
 * Resets the pack buffer to initial state.
 * @param pb - pointer to pack buffer
 * @param psize - new audio_pack size
 * @param byte_zero - BYTE0 of this session
 */
void pb_reset(pack_buffer *pb, uint64_t psize, uint64_t byte_zero);

/**
 * Tries to insert the @p pack into the buffer. Does nothing in case
 * @p psize differs from @p pb->psize.
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
 * Blocks if pack buffer @p pb is empty or haven't received a pack with
 * byte_num at least @p 0.75*pb->capacity apart from @p pb->byte_zero.
 *
 * If unable to take a pack from the buffer, it leaves @p item unchanged.
 * @param pb - pointer to pack buffer
 * @param item - result buffer
 * @returns psize of back buffer @p pb
 */
uint64_t pb_pop_front(pack_buffer *pb, void *item);

#endif //_PACK_BUFFER_
