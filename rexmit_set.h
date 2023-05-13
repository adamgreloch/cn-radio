#ifndef _REXMIT_QUEUE_
#define _REXMIT_QUEUE_

#include <netinet/in.h>

/**
 * Structure used for gathering a set of packs to retransmit.
 *
 * Because receivers are immune to packet reordering and will ignore the pack
 * if they already possess it, it is much more efficient (time/memory wise) to
 * remember a set of packs and retransmit them to multicast group all at
 * once after RTIME, than individually retransmitting specific packs to
 * receivers that asked for them - this would often involve retransmitting
 * the same pack to many receivers (unnecessary duplication and waste of time)
 * and drastically reduce time performance: many small single send sessions vs.
 * one big centralized multicast session. Besides, it would complicate the
 * retransmitting protocol, and its just easier to leave the job of delivery to
 * the network and make the whole procedure transparent to receivers.
 */
struct rexmit_set;

typedef struct rexmit_set rexmit_set;

void rs_add(rexmit_set *rs, uint64_t *packs, uint64_t n_packs);

void rs_pop_all(rexmit_set *rs, uint64_t *packs, uint64_t n_packs);

#endif //_REXMIT_QUEUE_
