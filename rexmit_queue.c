#include <pthread.h>
#include <unistd.h>
#include "rexmit_queue.h"

static bool debug = false;

typedef struct tree_node tree_node;

struct tree_node {
    tree_node *left;
    uint64_t num;
    tree_node *right;
};

tree_node *init_node(uint64_t num) {
    tree_node *node = malloc(sizeof(tree_node));
    node->num = num;
    node->left = node->right = NULL;
    return node;
}

tree_node *insert(tree_node *root, uint64_t num) {
    if (!root)
        return init_node(num);
    else if (root->num < num)
        root->left = insert(root->left, num);
    else if (root->num > num)
        root->right = insert(root->right, num);

    return root;
}

uint64_t tree_to_arr(tree_node *root, uint64_t **arr, uint64_t *arr_size,
                     uint64_t count) {
    if (root) {
        count = tree_to_arr(root->left, arr, arr_size, count);
        if (*arr_size == count) {
            if (*arr_size == 0) *arr_size = 1;
            else *arr_size *= 2;
            *arr = realloc(*arr, *arr_size * sizeof(uint64_t));
            if (!(*arr))
                fatal("realloc");
        }
        (*arr)[count++] = root->num;
        count = tree_to_arr(root->right, arr, arr_size, count);
    }
    return count;
}

void free_tree(tree_node *root) {
    if (root) {
        free_tree(root->left);
        free_tree(root->right);
        free(root);
    }
}

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

    tree_node *pack_tree;

    pthread_mutex_t mutex;
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

    rq->pack_tree = NULL;

    CHECK_ERRNO(pthread_mutex_init(&rq->mutex, NULL));
    return rq;
}

void rq_add_pack(rexmit_queue *rq, struct audio_pack *pack) {
    if (!rq || !pack) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));

    if (rq->head == rq->tail && rq->count > 0) {
        // delete tail elem
        rq->tail_byte_num += rq->psize;
        rq->count--;
        rq->tail += rq->psize;
        if (rq->tail + rq->psize >= rq->queue_end)
            rq->tail = rq->queue;
    }

    memcpy(rq->head, pack->audio_data, rq->psize);

    rq->head_byte_num = be64toh(pack->first_byte_num);
    rq->count++;
    rq->head += rq->psize;

    if (rq->head + rq->psize >= rq->queue_end)
        rq->head = rq->queue;

    if (debug)
        fprintf(stderr, "added pack %lu\n", rq->head_byte_num);

    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

static byte *_find_pack(rexmit_queue *rq, uint64_t first_byte_num) {
    uint64_t rel_pos = rq->head_byte_num - first_byte_num;
    byte *ptr = (rq->head - rq->psize) - rel_pos;

    if (ptr < rq->queue)
        ptr += rq->fsize;

    return ptr;
}

static void _bind_addr_to_pack(rexmit_queue *rq, uint64_t first_byte_num) {
    if (first_byte_num < rq->tail_byte_num ||
        first_byte_num > rq->head_byte_num) {
        if (debug)
            fprintf(stderr, "too late (%lu, %lu, %lu)\n", rq->tail_byte_num,
                    first_byte_num, rq->head_byte_num);
        return; // request invalid, ignore
    }

    rq->pack_tree = insert(rq->pack_tree, first_byte_num);

    if (debug)
        fprintf(stderr, "bound addr to %lu\n", first_byte_num);
}

void
rq_add_requests(rexmit_queue *rq, uint64_t *requested_packs, uint64_t n_packs) {
    if (!rq || !requested_packs) fatal("null argument");
    if (n_packs == 0) return;
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    for (size_t i = 0; i < n_packs; i++)
        _bind_addr_to_pack(rq, requested_packs[i]);
    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
}

uint64_t rq_get_requests(rexmit_queue *rq, uint64_t **requested_packs,
                         uint64_t *arr_size) {
    if (!rq) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    if (rq->count == 0) {
        CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
        return 0;
    }
    uint64_t count = tree_to_arr(rq->pack_tree, requested_packs, arr_size, 0);
    free_tree(rq->pack_tree);
    rq->pack_tree = NULL;

    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
    return count;
}

bool rq_get_pack(rexmit_queue *rq, byte *pack, uint64_t first_byte_num) {
    CHECK_ERRNO(pthread_mutex_lock(&rq->mutex));
    if (first_byte_num < rq->tail_byte_num ||
        first_byte_num > rq->head_byte_num) {
        CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
        return false;
    }
    byte *src = _find_pack(rq, first_byte_num);
    memcpy(pack, src, rq->psize);
    CHECK_ERRNO(pthread_mutex_unlock(&rq->mutex));
    return true;
}
