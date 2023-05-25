#include <pthread.h>
#include <unistd.h>
#include "rexmit_queue.h"

static bool debug = false;

struct addr_list {
    struct sockaddr_and_len addr;
    uint64_t first_byte_num;
    int rexmit_session;

    struct addr_list *next;
};

typedef struct addr_list addr_list;

struct rexmit_queue {
    byte *queue;
    byte *queue_end;

    byte *tail;
    byte *head;

    uint64_t head_byte_num; // byte_num of last inserted pack
    uint64_t tail_byte_num;

    uint64_t count;
    uint64_t fsize;
    uint64_t psize;

    addr_list *list_tail;
    addr_list *list_head;

    uint64_t list_len;
    int curr_rexmit_session;
    bool session_finished;

    pthread_mutex_t mutex;

    bool during_rexmit;
    pthread_cond_t rexmit_end_wait;
};

typedef struct rexmit_queue rexmit_queue;

rexmit_queue *rq_init(uint64_t psize, uint64_t fsize) {
    rexmit_queue *rq = malloc(sizeof(rexmit_queue));
    if (!rq)
        fatal("malloc");

    rq->queue = malloc(fsize);
    rq->queue_end = rq->queue + fsize;

    rq->head = rq->tail = rq->queue;
    rq->head_byte_num = rq->tail_byte_num = 0;

    rq->count = 0;
    rq->psize = psize;
    rq->fsize = fsize;
    rq->curr_rexmit_session = 0;
    rq->session_finished = true;
    rq->during_rexmit = false;

    rq->list_head = rq->list_tail = NULL;
    rq->list_len = 0;

    CHECK_ERRNO(pthread_mutex_init(&rq->mutex, NULL));
    CHECK_ERRNO(pthread_cond_init(&rq->rexmit_end_wait, NULL));
    return rq;
}

void rq_add_pack(rexmit_queue *rq, struct audio_pack *pack) {
    if (!rq || !pack) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));

    while (rq->during_rexmit)
        CHECK_ERRNO(pthread_cond_wait(&rq->rexmit_end_wait, &rq->mutex));

    if (rq->head == rq->tail && rq->count > 0) {
        // delete tail elem
        rq->tail_byte_num += rq->psize;
        rq->count--;
        rq->tail += rq->psize;
        if (rq->tail + rq->psize >= rq->queue_end)
            rq->tail = rq->queue;
    }

    memcpy(rq->head, pack->audio_data, rq->psize);

    rq->head_byte_num = ntohll(pack->first_byte_num);
    rq->count++;
    rq->head += rq->psize;

    if (rq->head + rq->psize >= rq->queue_end)
        rq->head = rq->queue;

    if (debug)
        fprintf(stderr, "added pack %lu (%lu, %lu)\n", rq->head_byte_num,
                rq->count, rq->list_len);

    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

byte *_find_pack(rexmit_queue *rq, uint64_t first_byte_num) {
    uint64_t rel_pos = rq->head_byte_num - first_byte_num;
    byte *ptr = (rq->head - rq->psize) - rel_pos;

    if (ptr < rq->queue)
        ptr += rq->fsize;

    return ptr;
}

void _bind_addr_to_pack(rexmit_queue *rq, uint64_t first_byte_num,
                        struct sockaddr_and_len *receiver_addr) {
    if (first_byte_num < rq->tail_byte_num ||
        first_byte_num > rq->head_byte_num) {
        if (debug)
            fprintf(stderr, "too late (%lu, %lu, %lu)\n", rq->tail_byte_num,
                    first_byte_num, rq->head_byte_num);
        return; // request invalid, ignore
    }

    addr_list *al = malloc(sizeof(addr_list));
    al->addr = *receiver_addr;
    al->next = NULL;
    al->first_byte_num = first_byte_num;

    al->rexmit_session = rq->curr_rexmit_session;

    if (rq->during_rexmit)
        al->rexmit_session = 1 - al->rexmit_session;

    if (!rq->list_tail)
        rq->list_tail = al;

    if (rq->list_head)
        rq->list_head->next = al;

    rq->list_head = al;
    rq->list_len++;

    if (debug)
        fprintf(stderr, "bound addr to %lu (%lu, %lu)\n", first_byte_num,
                rq->count, rq->list_len);
}

void rq_bind_addr(rexmit_queue *rq, uint64_t *packs, uint64_t n_packs, struct
        sockaddr_and_len *receiver_addr) {
    if (!rq || !packs) fatal("null argument");
    if (n_packs == 0) return;
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    for (size_t i = 0; i < n_packs; i++)
        _bind_addr_to_pack(rq, packs[i], receiver_addr);
    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

void rq_get_head_tail_byte_nums(rexmit_queue *rq, uint64_t *head_byte_num,
                                uint64_t *tail_byte_num) {
    if (!rq) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    *head_byte_num = rq->head_byte_num;
    *tail_byte_num = rq->tail_byte_num;
    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

addr_list *_pop_from_list(rexmit_queue *rq) {
    addr_list *popped = rq->list_tail;
    rq->list_tail = popped->next;
    rq->list_len--;
    if (!rq->list_tail)
        rq->list_head = NULL;

    return popped;
}

bool rq_pop_pack_for_addr(rexmit_queue *rq, byte *pack_data,
                          uint64_t *first_byte_num,
                          struct sockaddr_and_len *receiver_addr) {
    if (!rq) fatal("null argument");

    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    if (rq->count == 0 || rq->list_len == 0 || rq->session_finished) {
        if (rq->session_finished)
            rq->session_finished = false;
        CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
        return false;
    }

    if (!rq->during_rexmit)
        rq->during_rexmit = true;

    addr_list *al = NULL;
    bool pack_ready = false;

    while (!pack_ready && rq->list_len > 0) {
        al = _pop_from_list(rq);
        if (al && rq->tail_byte_num <= al->first_byte_num
            && al->first_byte_num <= rq->head_byte_num) {
            memcpy(pack_data, _find_pack(rq, al->first_byte_num),
                   rq->psize);
            *first_byte_num = al->first_byte_num;
            pack_ready = true;
            if (debug)
                fprintf(stderr, "popped pack (%lu, %lu, sess=%d)\n", rq->count,
                        rq->list_len, rq->curr_rexmit_session);
        }
        if (rq->list_len == 0 ||
            al->rexmit_session != rq->curr_rexmit_session) {
            rq->session_finished = true;
            rq->during_rexmit = false;
            rq->curr_rexmit_session = 1 - rq->curr_rexmit_session;
            CHECK_ERRNO(pthread_cond_signal(&rq->rexmit_end_wait));
            break;
        }
    }

    if (al)
        *receiver_addr = al->addr;

    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));

    return pack_ready;
}
