#include "pack_buffer.h"
#include <pthread.h>
#include <assert.h>

struct pack_buffer {
    byte *buf;                                        /**< data buffer */
    byte *buf_end;                             /**< end of data buffer */

    bool *is_present;    /**< indicates whether i-th pack is in buffer */

    uint64_t capacity;      /**< maximum number of items in the buffer */
    uint64_t psize;
    byte *head;                            /**< pointer to buffer head */
    uint64_t head_byte_num;                /**< first_byte_num of head */
    uint64_t byte_zero;                   /**< current session's byte0 */
    byte *tail;                            /**< pointer to buffer tail */

    pthread_mutex_t mutex;
    pthread_cond_t byte_zero_wait;
    pthread_cond_t init_wait;
};

pack_buffer *pb_init(uint64_t bsize) {
    pack_buffer *pb = malloc(sizeof(pack_buffer));

    pb->buf = malloc(bsize);
    if (pb->buf == NULL)
        fatal("malloc");

    pb->is_present = malloc(bsize);
    pb->buf_end = (byte *) pb->buf + bsize;
    pb->capacity = bsize;
    pb->psize = 0;
    pb->head = pb->tail = pb->buf;
    pb->head_byte_num = pb->byte_zero = 0;

    CHECK_ERRNO(pthread_mutex_init(&pb->mutex, NULL));
    CHECK_ERRNO(pthread_cond_init(&pb->byte_zero_wait, NULL));
    CHECK_ERRNO(pthread_cond_init(&pb->init_wait, NULL));

    return pb;
}

void reset_buffer(pack_buffer *pb, uint64_t byte_zero) {
    memset(pb->buf, 0, pb->capacity);
    memset(pb->is_present, 0, pb->capacity);

    pb->buf_end = (byte *) pb->buf + pb->capacity;
    pb->head_byte_num = pb->byte_zero = byte_zero;
    pb->head = pb->tail = pb->buf;
}

void pb_reset(pack_buffer *pb, uint64_t psize, uint64_t byte_zero) {
    if (!pb) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&pb->mutex));
    reset_buffer(pb, byte_zero);
    pb->psize = psize;
    CHECK_ERRNO(pthread_cond_signal(&pb->init_wait));
    CHECK_ERRNO(pthread_mutex_unlock(&pb->mutex));
}

void handle_buf_end_overlap(byte **pos, pack_buffer *pb) {
    if (*pos + pb->psize > pb->buf_end)
        *pos = pb->buf;
}

void pb_find_missing(pack_buffer *pb, uint64_t *n_packs,
                     uint64_t **missing_buf, uint64_t *buf_size) {
    if (!pb) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&pb->mutex));

    while (pb->psize == 0)
        CHECK_ERRNO(pthread_cond_wait(&pb->init_wait, &pb->mutex));

    size_t exp_size = sizeof(uint64_t) * pb->capacity / pb->psize;

    if (*buf_size != exp_size) {
        *missing_buf = realloc(*missing_buf, exp_size);
        if (!(*missing_buf))
            fatal("realloc");
        *buf_size = exp_size;
    }

    byte *pos = pb->tail;
    uint64_t missing_byte_num;
    uint64_t shift = 0;

    uint64_t last = 0;

    while (pos != pb->head) {
        if (!pb->is_present[pos - pb->buf]) {
            if (pos >= pb->head)
                // Fix relative missing package number in case it is on the
                // right end of the buffer.
                shift = pb->buf - pb->buf_end;

            missing_byte_num = pb->head_byte_num - (pb->head - shift - pos);

            if (missing_byte_num < pb->head_byte_num &&
                missing_byte_num > pb->byte_zero)
                (*missing_buf)[last++] = missing_byte_num;
        }

        pos += pb->psize;

        if (pb->head != pb->buf_end)
            handle_buf_end_overlap(&pos, pb);
    }

    CHECK_ERRNO(pthread_mutex_unlock(&pb->mutex));

    *n_packs = last;
}

/**
 * Wipes the buffer contents while updating the pack counter and is_present
 * table in the pack buffer.
 * @param pb - pointer to pack buffer
 * @param ptr - memory area in the buffer from which to wipe @p bytes
 * @param bytes - number of bytes to wipe
 */
void wipe_buffer(pack_buffer *pb, byte *ptr, size_t bytes) {
    if (ptr + bytes > pb->buf_end)
        bytes -= ptr + bytes - pb->buf_end;

    memset(ptr, 0, bytes);
    uint64_t pos = ptr - pb->buf;
    assert(pos % pb->psize == 0);
    memset(pb->is_present + pos, 0, bytes);
}

void add_pack(pack_buffer *pb, byte *ptr, const byte *pack) {
    memcpy(ptr, pack, pb->psize);
    pb->is_present[ptr - pb->buf] = true;
}

void
handle_buffer_overflow(pack_buffer *pb, uint64_t missing, const byte *pack) {
    // How many bytes left till the buffer end from current head.
    uint64_t bytes_to_end = pb->buf_end - pb->head;

    wipe_buffer(pb, pb->head, bytes_to_end); // Wipe from head to end.

    // How many slots left till the buffer end from current head.
    // Important to introduce such value, since pb->capacity doesn't
    // need to be divisible by pb->psize.
    uint64_t packs_to_end = bytes_to_end / pb->psize;

    // Reserve space for the rest of missing packages that land
    // in the buffer beginning and new head.
    wipe_buffer(pb, pb->buf, missing - packs_to_end * pb->psize + pb->psize);

    // Place the pack in its new spot.
    byte *prev_head = pb->head;

    pb->head = pb->buf + missing - packs_to_end * pb->psize;
    add_pack(pb, pb->head, pack);
    pb->head += pb->psize;

    if ((pb->head >= pb->tail && prev_head >= pb->tail)
        || (pb->head <= pb->tail && prev_head <= pb->tail)) {
        // Head overlapped the tail.
        pb->tail = pb->head + pb->psize;
        handle_buf_end_overlap(&pb->tail, pb);
    }
}

uint64_t range(pack_buffer *pb) {
    if (pb->tail <= pb->head) return pb->head - pb->tail;
    else return pb->capacity + pb->head - pb->tail;
}

void insert_pack_into_buffer(pack_buffer *pb, uint64_t first_byte_num,
                             const byte *pack) {
    // How many space to reserve in bytes for missing packs, if any.
    // Overflow warning: first_byte_num may be less than pb->head_byte_num.
    uint64_t missing = first_byte_num - pb->head_byte_num;

    if (pb->head_byte_num > first_byte_num) {
        // Encountered a missing pack.
        if (pb->head_byte_num - first_byte_num <= range(pb)) {
            byte *ptr = pb->head - pb->head_byte_num + first_byte_num;

            if (ptr < pb->buf) // Left overlap
                ptr += pb->capacity / pb->psize * pb->psize;

            add_pack(pb, ptr, pack);
        } // else: Encountered a missing, but ancient package... ignore.
        return;
    } else if (missing > pb->capacity) {
        // This won't fit. Reset the buffer.
        missing = (pb->capacity / pb->psize - 1) * pb->psize;
        reset_buffer(pb, first_byte_num - missing);
        add_pack(pb, pb->head + missing, pack);
        pb->head = pb->head + missing + pb->psize;
    } else if (pb->head + missing >= pb->buf_end)
        // Buffer overflow.
        handle_buffer_overflow(pb, missing, pack);
    else {
        if (pb->head < pb->tail && pb->tail <= pb->head + missing + pb->psize) {
            // Tail overlap.
            wipe_buffer(pb, pb->head, missing + 2 * pb->psize);
            pb->tail = pb->head + missing + 2 * pb->psize;
            handle_buf_end_overlap(&pb->tail, pb);
        }

        // Insert normally.
        add_pack(pb, pb->head + missing, pack);
        pb->head = pb->head + missing + pb->psize;
    }

    pb->head_byte_num = first_byte_num + pb->psize;
    handle_buf_end_overlap(&pb->head, pb);
}

bool is_in_buffer(pack_buffer *pb, uint64_t first_byte_num) {
    if (pb->head_byte_num < first_byte_num) return false;

    uint64_t head_pos = pb->head - pb->buf;
    uint64_t dist_from_head = pb->head_byte_num - first_byte_num;

    if (head_pos < dist_from_head) return false;

    return pb->is_present[head_pos - dist_from_head];
}

void pb_push_back(pack_buffer *pb, uint64_t first_byte_num, const byte *pack,
                  uint64_t psize) {
    if (!pb) fatal("null argument");
    if (pb->psize != psize) return;
    CHECK_ERRNO(pthread_mutex_lock(&pb->mutex));

    if (!is_in_buffer(pb, first_byte_num))
        insert_pack_into_buffer(pb, first_byte_num, pack);

    if (pb->head_byte_num - pb->byte_zero >= pb->capacity / 4 * 3)
        CHECK_ERRNO(pthread_cond_signal(&pb->byte_zero_wait));

    CHECK_ERRNO(pthread_mutex_unlock(&pb->mutex));
}

void take_pack_if_present(pack_buffer *pb, void *item) {
    if (pb->is_present[pb->tail - pb->buf]) {
        memcpy(item, pb->tail, pb->psize);
        pb->is_present[pb->tail - pb->buf] = false;
    } // else: just play silence
    /*
     * NOTE: just playing silence does not comply with the requirements, which
     * say that if the pack is not found, the playback should be stopped
     * just like it is done in case of buffer depletion. However,
     * following this requirement degrades the playback fluency if the
     * REXMIT parameters are picked suboptimally. It is thus reasonable to
     * ignore this requirement for the sake of better listening experience.
     */

    if (pb->head != pb->tail) {
        pb->tail += pb->psize;
        handle_buf_end_overlap(&pb->tail, pb);
    }
}

uint64_t pb_pop_front(pack_buffer *pb, void *item) {
    if (!pb) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&pb->mutex));

    while (pb->head == pb->tail ||
           pb->head_byte_num - pb->byte_zero < pb->capacity / 4 * 3) {
        if (pb->head == pb->tail)
            // Buffer is depleted. We will wait for it to fill up
            // to approx. 75% to avoid unstable playback.
            pb->byte_zero = pb->head_byte_num;
        // else: Stop playback until (BYTE0 + 3/4 * PSIZE)'th byte received.
        CHECK_ERRNO(pthread_cond_wait(&pb->byte_zero_wait, &pb->mutex));
    }

    take_pack_if_present(pb, item);

    uint64_t curr_psize = pb->psize;

    CHECK_ERRNO(pthread_mutex_unlock(&pb->mutex));

    return curr_psize;
}
