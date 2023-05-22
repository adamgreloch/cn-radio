#include "rexmit_queue.h"

int main() {
    int psize = 4;
    int fsize = 25;
    rexmit_queue *rq = rq_init(psize, fsize);
    struct audio_pack pack;
    byte *data = malloc(psize);
    pack.session_id = 0;

    for (int i = 0; i < 25; i++) {
        pack.first_byte_num = psize * i;
        sprintf((char*) data, "%d", psize * i);
        pack.audio_data = data;
        rq_add_pack(rq, &pack, psize * i);
    }

    uint64_t n_packs = 5;
    uint64_t packs[n_packs];
    for (int i = 0; i < n_packs; i++)
        packs[i] = psize * i;

    sockaddr_and_len receiver_addr;
    receiver_addr.addr.sin_addr.s_addr = 1234;
    receiver_addr.addr.sin_port = 5678;
    receiver_addr.addr.sin_family = 42;
    receiver_addr.addr_len = (socklen_t) sizeof(receiver_addr.addr);

    rq_bind_addr(rq, packs, n_packs, &receiver_addr); // should do nothing

    for (int i = 0; i < n_packs; i++)
        packs[i] = psize * (i + 18);

    rq_bind_addr(rq, packs, n_packs, &receiver_addr); // should bind

    receiver_addr.addr.sin_addr.s_addr = 1235;

    rq_bind_addr(rq, packs, n_packs, &receiver_addr); // should bind and extend
    // lists

    sockaddr_and_len* receiver_addrs = NULL;
    uint64_t addr_count = 0;

    uint64_t head_bn, tail_bn;

    rq_get_head_tail_byte_nums(rq, &head_bn, &tail_bn);

    struct audio_pack res;

    for (uint64_t bn = tail_bn; bn <= head_bn; bn += psize) {
        rq_peek_pack_with_addrs(rq, bn, &res, &receiver_addrs, &addr_count);
    }

    for (int i = 25; i < 50; i++) {
        pack.first_byte_num = psize * i;
        sprintf((char*) data, "%d", psize * i);
        pack.audio_data = data;
        rq_add_pack(rq, &pack, psize * i);
    }
}