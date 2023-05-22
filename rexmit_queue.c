#include "rexmit_queue.h"
#include <pthread.h>

// TODO null proof the code

struct addr_list {
    struct sockaddr_and_len addr;
    struct addr_list *next;
};

typedef struct addr_list addr_list;

struct queue_elem {
    byte *data;

    addr_list *tail;
    addr_list *head;

    uint32_t list_len;
};

typedef struct queue_elem queue_elem;

struct rexmit_queue {
    queue_elem **queue;
    queue_elem **queue_end;

    queue_elem **tail;
    queue_elem **head;

    uint64_t head_byte_num; // byte_num of last inserted pack
    uint64_t tail_byte_num;

    uint64_t count;
    uint64_t fsize;
    uint64_t psize;
    uint64_t queue_size;

    pthread_mutex_t mutex;
};

void _free_addr_list(queue_elem *qe) {
    if (qe->list_len == 0) return;
    addr_list *curr = qe->tail;
    addr_list *next;
    while (curr) {
        next = curr->next;
        free(curr);
        curr = next;
    }
    qe->list_len = 0;
    qe->tail = qe->head = NULL;
}

rexmit_queue *rq_init(uint64_t psize, uint64_t fsize) {
    rexmit_queue *rq = malloc(sizeof(rexmit_queue));
    if (!rq)
        fatal("malloc");
    rq->queue_size = fsize / psize;
    rq->queue = malloc(sizeof(queue_elem *) * rq->queue_size);
    rq->queue_end = rq->queue + rq->queue_size;

    rq->head = rq->tail = rq->queue;
    rq->head_byte_num = rq->tail_byte_num = 0;

    rq->count = 0;
    rq->psize = psize;
    rq->fsize = fsize;

    CHECK_ERRNO(pthread_mutex_init(&rq->mutex, NULL));
    return rq;
}

void rq_add_pack(rexmit_queue *rq, byte *pack_data, uint64_t first_byte_num) {
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    if (!(*rq->head)) {
        *rq->head = malloc(sizeof(queue_elem));
        (*rq->head)->data = malloc(rq->psize);
    }

    if (rq->head == rq->tail && rq->count > 0) {
        // delete tail elem
        _free_addr_list(*rq->tail);
        rq->tail_byte_num += rq->psize;
        rq->count--;
        rq->tail++;
        if (rq->tail >= rq->queue_end)
            rq->tail = rq->queue;
    }

    queue_elem *qe = *rq->head;
    memcpy(qe->data, pack_data, rq->psize);
    qe->tail = qe->head = NULL;

    rq->head_byte_num = first_byte_num;

    rq->count++;
    rq->head++;
    if (rq->head >= rq->queue_end)
        rq->head = rq->queue;

    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

queue_elem *_find_elem(rexmit_queue *rq, uint64_t first_byte_num) {
    uint64_t rel_pos = (rq->head_byte_num - first_byte_num) / rq->psize;
    queue_elem **ptr =
            (rq->head - 1) - rel_pos; // head-1 is a last inserted element
    if (ptr < rq->queue)
        ptr += rq->queue_size;

    return (*ptr);
}

void _bind_addr_to_pack(rexmit_queue *rq, uint64_t first_byte_num,
                        struct sockaddr_and_len *receiver_addr) {
    if (first_byte_num < rq->tail_byte_num ||
        first_byte_num > rq->head_byte_num)
        return; // request invalid, ignore

    queue_elem *qe = _find_elem(rq, first_byte_num);
    addr_list *al = malloc(sizeof(addr_list));
    al->addr = *receiver_addr;
    al->next = NULL;

    if (!qe->tail)
        qe->tail = al;

    if (qe->head)
        qe->head->next = al;

    qe->head = al;
    qe->list_len++;
}

void rq_bind_addr(rexmit_queue *rq, uint64_t *packs, uint64_t n_packs, struct
        sockaddr_and_len *receiver_addr) {
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    for (size_t i = 0; i < n_packs; i++)
        _bind_addr_to_pack(rq, packs[i], receiver_addr);
    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

void rq_get_head_tail_byte_nums(rexmit_queue *rq, uint64_t *head_byte_num,
                                uint64_t *tail_byte_num) {
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    *head_byte_num = rq->head_byte_num;
    *tail_byte_num = rq->tail_byte_num;
    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

addr_list *_pop_from_list(queue_elem *qe) {
    addr_list *popped = qe->tail;
    qe->tail = popped->next;
    qe->list_len--;
    if (!qe->tail)
        qe->head = NULL;

    return popped;
}

void rq_peek_pack_with_addrs(rexmit_queue *rq, uint64_t first_byte_num, byte
*pack_data, struct sockaddr_and_len **receiver_addrs, uint64_t *addr_count) {
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    queue_elem *qe = _find_elem(rq, first_byte_num);
    if (*addr_count < qe->list_len) {
        *receiver_addrs = realloc(*receiver_addrs, qe->list_len * sizeof
                (receiver_addrs));
        *addr_count = qe->list_len;
    }

    addr_list *al;
    size_t i = 0;

    while (qe->list_len > 0) {
        al = _pop_from_list(qe);
        (*receiver_addrs)[i++] = al->addr;
    }

    memcpy(pack_data, qe->data, rq->psize);
    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}
