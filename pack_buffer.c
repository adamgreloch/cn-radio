#include <pthread.h>
#include "pack_buffer.h"

typedef struct pack_buffer {
    byte *buffer;     // data buffer
    byte *buffer_end; // end of data buffer

    bool *is_present; // indicates whether i-th pack is present or missing

    size_t capacity;  // maximum number of items in the buffer
    uint64_t psize;
    size_t count;     // number of items in the buffer
    byte *head;       // pointer to head
    uint64_t head_byte_num;
    byte *tail;       // pointer to tail

    pthread_mutex_t mutex;
    pthread_cond_t wait_for_fill;
    pthread_cond_t wait_for_deplete;
} pack_buffer;


pack_buffer *pb_init(size_t bsize) {
    pack_buffer *pb = malloc(sizeof(pack_buffer));
    pb->buffer = malloc(bsize);

    if (pb->buffer == NULL)
        fatal("malloc");

    pb->is_present = malloc(bsize);

    pb->buffer_end = (byte *) pb->buffer + bsize;
    pb->capacity = bsize;
    pb->psize = 0;
    pb->count = 0;
    pb->head = pb->tail = pb->buffer;
    pb->head_byte_num = 0;

    CHECK_ERRNO(pthread_mutex_init(&pb->mutex, NULL));
    CHECK_ERRNO(pthread_cond_init(&pb->wait_for_fill, NULL));
    CHECK_ERRNO(pthread_cond_init(&pb->wait_for_deplete, NULL));
    return pb;
}

void pb_free(pack_buffer *pb) {
    free(pb->buffer);
    free(pb->is_present);
    free(pb);
}

void pb_reset(pack_buffer *pb, uint64_t psize) {
    CHECK_ERRNO(pthread_mutex_lock(&pb->mutex));
    memset(pb->buffer, 0, pb->capacity);
    memset(pb->is_present, 0, pb->capacity);

    pb->buffer_end = (byte *) pb->buffer + pb->capacity;
    pb->psize = psize;
    pb->count = 0;
    pb->head_byte_num = 0;

    pb->head = pb->tail = pb->buffer;
    CHECK_ERRNO(pthread_mutex_unlock(&pb->mutex));
}

void pb_push_back(pack_buffer *pb, uint64_t first_byte_num, const byte *pack,
                  uint64_t psize) {
    CHECK_ERRNO(pthread_mutex_lock(&pb->mutex));
    if (pb->psize != psize) {}

    while (pb->count * psize == pb->capacity)
        CHECK_ERRNO(pthread_cond_wait(&pb->wait_for_deplete, &pb->mutex));

    if (pb->head_byte_num > first_byte_num) {
        // encountered a missing pack
        memcpy(pb->head + first_byte_num - pb->head_byte_num, pack, psize);
        pb->count++;
        fprintf(stderr, "MISSING PACK FOUND\n");
        return;
    }

    // reserve space for missing packs if there are any (if missing > 0)
    uint64_t missing = first_byte_num - pb->head_byte_num;

    fprintf(stderr, "%lu MISSING BYTES\n", missing);

    // TODO check exactly what packs are missing
    //  add bool is_present table that gets dynamically reallocated
    //  on pb_reset() call

    if (pb->head + missing >= pb->buffer_end) {
        // buffer overflow
        if (pb->buffer + (missing % pb->capacity) + psize >= pb->tail) {
            // head would overlap the tail
            fprintf(stderr, "OVERFLOW TAIL OVERLAP\n");

            // TODO
        } else {
            fprintf(stderr, "OVERFLOW\n");
            memcpy(pb->head + (missing % pb->capacity), pack, psize);
            pb->head = pb->buffer + (missing % pb->capacity) + psize;
        }
    } else {
        fprintf(stderr, "CAPACITY OK\n");
        memcpy(pb->head + missing, pack, psize);
        pb->head = (byte *) pb->head + missing + psize;

        if (pb->head == pb->buffer_end)
            pb->head = pb->buffer;
    }

    pb->head_byte_num = first_byte_num + psize;

    uint64_t pack_num = (&pb->head - &pb->buffer) / psize - 1;
    pb->is_present[pack_num] = true;
    pb->count++;

    if (pb->count * psize >= pb->capacity / 4 * 3)
        CHECK_ERRNO(pthread_cond_signal(&pb->wait_for_fill));

    CHECK_ERRNO(pthread_mutex_unlock(&pb->mutex));
}

void pb_pop_front(pack_buffer *pb, void *item, uint64_t psize) {
    CHECK_ERRNO(pthread_mutex_lock(&pb->mutex));
    if (pb->psize != psize) {}

    while (pb->count * psize < pb->capacity / 4 * 3)
        CHECK_ERRNO(pthread_cond_wait(&pb->wait_for_fill, &pb->mutex));

    memcpy(item, pb->tail, psize);
    pb->tail = (byte *) pb->tail + psize;

    if (pb->tail == pb->buffer_end)
        pb->tail = pb->buffer;

    pb->count--;

    if (pb->count * psize < pb->capacity)
        CHECK_ERRNO(pthread_cond_signal(&pb->wait_for_deplete));

    CHECK_ERRNO(pthread_mutex_unlock(&pb->mutex));
}

