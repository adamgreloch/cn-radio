#ifndef _PACK_BUFFER_
#define _PACK_BUFFER_

#include <stdlib.h>
#include <stdint.h>
#include "common.h"

struct pack_buffer;

typedef struct pack_buffer pack_buffer;

pack_buffer *pb_init(size_t bsize);

void pb_free(pack_buffer *pb);

void pb_reset(pack_buffer *pb, uint64_t psize, uint64_t byte_zero);

void pb_push_back(pack_buffer *pb, uint64_t first_byte_num, const byte *pack,
                  uint64_t psize);

bool pb_pop_front(pack_buffer *pb, void *item, uint64_t psize);

#endif //_PACK_BUFFER_
