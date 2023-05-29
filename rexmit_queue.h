#ifndef _REXMIT_QUEUE_
#define _REXMIT_QUEUE_

#include <netinet/in.h>
#include "common.h"

/**
 * A structure holding a queue of packs and pending requests of their
 * retransmission. Queue stores at most FSIZE bytes of packs.
 */
struct rexmit_queue;

typedef struct rexmit_queue rexmit_queue;

struct sockaddr_and_len {
    struct sockaddr_in addr;
    socklen_t addr_len;
};

typedef struct sockaddr_and_len sockaddr_and_len;

/**
 * Initializes rexmit queue
 * @param psize - value of PSIZE
 * @param fsize - value of FSIZE
 * @returns pointer to rexmit queue
 */
rexmit_queue *rq_init(uint64_t psize, uint64_t fsize);

/**
 * Adds @p receiver_addr address' requests for retransmission.
 * @param rq - pointer to rexmit queue
 * @param requested_packs - array of first_byte_nums of requested_packs requested
 * @param n_packs - number of elements in @p requested_packs
 * @param receiver_addr - address of the receiver that issued the requested
 */
void
rq_add_requests(rexmit_queue *rq, uint64_t *requested_packs, uint64_t n_packs);

/**
 * Gets an array with first_byte_nums of packs that were requested for
 * retransmission. Note that is not guaranteed that all the packs are still in
 * the queue and can be recovered with rq_get_pack().
 *
 * @param rq - pointer to rexmit queue
 * @param requested_packs - pointer to array of requested first_byte_nums
 * @param arr_size - pointer to size of the @p requested_packs
 * @returns number of first_byte_nums in the array
 */
uint64_t rq_get_requests(rexmit_queue *rq, uint64_t **requested_packs, uint64_t
*arr_size);

/**
 * Pops a pack data with the specified @p first_byte_num to @p pack_data buffer.
 *
 * @param rq - pointer to rexmit queue
 * @param pack_data - audio_data buffer
 * @param first_byte_num - byte_num of the pack to pop
 * @returns true if popped successfully; false if pack not found
 */
bool rq_get_pack(rexmit_queue *rq, byte *pack_data, uint64_t first_byte_num);

/**
 * Adds a pack to the queue. If there is not enough space in the queue,
 * removes the oldest pack from the queue. Assumes pack has exactly
 * PSIZE bytes.
 * @param rq - pointer to rexmit queue
 * @param pack - pack to insert
 */
void rq_add_pack(rexmit_queue *rq, struct audio_pack *pack);

#endif //_REXMIT_QUEUE_
