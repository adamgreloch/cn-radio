#ifndef _REXMIT_QUEUE_
#define _REXMIT_QUEUE_

#include <netinet/in.h>

/**
 * A structure holding a set of pack buffers (one buffer per listening
 * receiver) each capable of storing FSIZE/PSIZE pack numbers.
 */
struct rexmit_set;

typedef struct rexmit_set rexmit_set;

/**
 * Adds packs from @p packs to receiver's packs queue.
 */
void rs_add(rexmit_set *rs, uint64_t *packs, uint64_t n_packs, struct
sockaddr_in* receiver_addr, socklen_t addr_len);

/**
 * Pops a queue of a receiver, stores his address in @p receiver_addr and
 * pack numbers he requested for retransmission in @p packs.
 */
void rs_pop_all_for_addr(rexmit_set *rs, uint64_t *packs, uint64_t n_packs,
                         struct sockaddr_in *receiver_addr, socklen_t addr_len);

#endif //_REXMIT_QUEUE_
