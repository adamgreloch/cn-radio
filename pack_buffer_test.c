#include "pack_buffer.h"
#include "common.h"
#include <stdio.h>
#include <assert.h>

byte pack[1];
byte res[1];

void insert_pack(pack_buffer *pb, uint8_t num, uint64_t psize) {
    pack[0] = num;
    pb_push_back(pb, num, pack, psize);
}

uint8_t get_pop_result(pack_buffer *pb, uint64_t psize) {
    pb_pop_front(pb, res, psize);
    return res[0];
}

int main() {
    uint64_t psize = 1;
    uint64_t bsize = 10;
    pack_buffer *pb = pb_init(bsize);

    pb_reset(pb, psize, 0);
    fprintf(stderr, "TEST 1\n");

    // Sending packs:
    // 0 1 2 3 6 7 8 9
    for (int i = 0; i < 4; i++)
        insert_pack(pb, i, psize);

    for (int i = 6; i < 10; i++)
        insert_pack(pb, i, psize);

    // Should detect missing packs with numbers:
    // 4 5
    // when inserting:
    // 6 7 8 9

    pb_reset(pb, psize, 0);
    fprintf(stderr, "TEST 2\n");

    // Sending packs:
    // 0 1 2 3 4 5 6 7 15
    for (int i = 0; i < 8; i++)
        insert_pack(pb, i, psize);


    insert_pack(pb, 15, psize);
    // Should detect missing packs with numbers 8-14.

    // Overflow expected. There should be just pack 7 in the buffer, since
    // new head points to space occupied previously by 6.
    pb_pop_front(pb, res, psize);
    assert(res[0] == 7);

    pb_reset(pb, psize, 0);
    fprintf(stderr, "TEST 3\n");

    // Sending packs:
    // 0 1 2 3 4 5 6 7 18

    for (int i = 0; i < 8; i++)
        insert_pack(pb, i, psize);

    insert_pack(pb, 18, psize);

    // Should detect missing packs with numbers 8-17.

    // Overflow expected. There should be just pack 18 in the buffer.
    assert(pb_count(pb) == 1);

    // Now, when inserting 17, packs 10-16 should be detected as missing.
    // Only those can fit into the buffer (8 and 9 not anymore).
    insert_pack(pb, 17, psize);
    assert(pb_count(pb) == 2);

    // Nothing should be missing when inserting 10 and 11.
    insert_pack(pb, 10, psize);
    assert(pb_count(pb) == 3);
    insert_pack(pb, 11, psize);
    assert(pb_count(pb) == 4);

    assert(get_pop_result(pb, psize) == 10);
    assert(get_pop_result(pb, psize) == 11);
    assert(get_pop_result(pb, psize) == 0);

    // 13 should be missing when inserting 14.
    insert_pack(pb, 14, psize);

    assert(get_pop_result(pb, psize) == 0);
    assert(get_pop_result(pb, psize) == 14);

    // 15 should be missing when inserting 16.
    insert_pack(pb, 16, psize);

    // Duplicated packs should be ignored.
    insert_pack(pb, 16, psize);
}