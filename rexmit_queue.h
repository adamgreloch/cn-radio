#ifndef _REXMIT_QUEUE_
#define _REXMIT_QUEUE_

#include <netinet/in.h>
#include "common.h"

/**
 * A structure holding a queue of packs. Queue stores FSIZE bytes of packs.
 * Each pack in queue has a list of receiver addresses that have requested
 * its retransmission. Assumes FSIZE >= PSIZE.
 */
struct rexmit_queue;

typedef struct rexmit_queue rexmit_queue;

struct sockaddr_and_len {
    struct sockaddr_in addr;
    socklen_t addr_len;
};

typedef struct sockaddr_and_len sockaddr_and_len;

rexmit_queue *rq_init(uint64_t psize, uint64_t fsize);

/**
 * Binds @p receiver_addr address to lists possessed by packs from @p packs,
 * if they are in the queue.
 */
void rq_bind_addr(rexmit_queue *rq, uint64_t *packs, uint64_t n_packs, struct
sockaddr_and_len* receiver_addr);

/**
 * Pops a pack with the addresses that have requested its retransmission.
 */
bool rq_pop_pack_for_addr(rexmit_queue *rq, byte *pack_data,
                          uint64_t *first_byte_num,
                          struct sockaddr_and_len *receiver_addr);
/**
 * Adds a pack to the queue. If there is not enough space in the queue,
 * removes the oldest pack from the queue. Assumes pack has exactly
 * PSIZE bytes.
 */
void rq_add_pack(rexmit_queue *rq, struct audio_pack *pack);

void rq_get_head_tail_byte_nums(rexmit_queue *rq, uint64_t *head_byte_num,
                                uint64_t *tail_byte_num);
#endif //_REXMIT_QUEUE_
