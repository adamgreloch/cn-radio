#include "pack_buffer.h"

char *psize_errmsg = "psize arg doesn't match buffer's psize";

struct pack_buffer {
    byte *buf;         /**< data buffer */
    byte *buf_end;     /**< end of data buffer */

    bool *is_present;  /**< indicates whether i-th pack is present or missing */

    uint64_t capacity;   /**< maximum number of items in the buffer */
    uint64_t psize;
    uint64_t count;      /**< number of items in the buffer */
    byte *head;        /**< pointer to head */
    uint64_t head_byte_num; /**< first_byte_num of head */
    uint64_t byte_zero;     /**< current session's byte0 */
    byte *tail;        /**< pointer to tail */
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
    pb->count = 0;
    pb->head = pb->tail = pb->buf;
    pb->head_byte_num = pb->byte_zero = 0;

    return pb;
}

void pb_reset(pack_buffer *pb, uint64_t psize, uint64_t byte_zero) {
    memset(pb->buf, 0, pb->capacity);
    memset(pb->is_present, 0, pb->capacity);

    pb->buf_end = (byte *) pb->buf + pb->capacity;
    pb->psize = psize;
    pb->count = 0;

    pb->head_byte_num = pb->byte_zero = byte_zero;
    pb->head = pb->tail = pb->buf;
}

void handle_buf_end_overlap(byte **pos, pack_buffer *pb) {
    if (*pos + pb->psize > pb->buf_end)
        *pos = pb->buf;
}

/**
 * Finds all packs older than @p first_byte_num pack that could fit into the
 * buffer and are not present and prints their byte numbers to STDERR in
 * increasing order.
 * @param pb - pointer to pack buffer
 * @param first_byte_num - byte number identifying the pack of interest
 */
void find_missing(pack_buffer *pb, uint64_t first_byte_num) {
    byte *pos = pb->tail;
    uint64_t missing_byte_num;
    uint64_t shift = 0;

    while (pos != pb->head) {
        if (!pb->is_present[pos - pb->buf]) {
            if (pos >= pb->head)
                // Fix relative missing package number in case it is on the
                // right end of the buffer.
                shift = pb->buf - pb->buf_end;

            missing_byte_num = pb->head_byte_num - (pb->head - shift - pos);

            fprintf(stderr, "MISSING: BEFORE %lu EXPECTED %lu\n",
                    first_byte_num, missing_byte_num);
        }

        pos += pb->psize;

        if (pb->head != pb->buf_end)
            handle_buf_end_overlap(&pos, pb);
    }
}

uint64_t
handle_buffer_overflow(pack_buffer *pb, uint64_t missing, const byte *pack) {
    // How many bytes left till the buffer end from current head.
    uint64_t bytes_to_end = pb->buf_end - pb->head;

    memset(pb->head, 0, bytes_to_end); // Wipe from head to end.

    // How many slots left till the buffer end from current head.
    // Important to introduce such value, since pb->capacity doesn't
    // need to be divisible by pb->psize.
    uint64_t packs_to_end = bytes_to_end / pb->psize;

    // Reserve space for the rest of missing packages that land
    // in the buffer beginning.
    memset(pb->buf, 0, missing - packs_to_end * pb->psize);

    // Place the pack in its new spot.
    pb->head = pb->buf + missing - packs_to_end * pb->psize;
    memcpy(pb->head, pack, pb->psize);
    pb->head += pb->psize;

    if (pb->head >= pb->tail) {
        // Head overlapped the tail.
        memset(pb->tail, 0, pb->head - pb->tail);
        pb->tail = pb->head + pb->psize;
        handle_buf_end_overlap(&pb->tail, pb);
    }

    // Return pack_num
    return pb->head - pb->buf - pb->psize;
}

uint64_t normal_insert(pack_buffer *pb, uint64_t missing, const byte *pack) {
    memcpy(pb->head + missing, pack, pb->psize);

    pb->head = (byte *) pb->head + missing + pb->psize;
    uint64_t pack_num = pb->head - pb->buf - pb->psize;

    return pack_num;
}

uint64_t insert_pack_into_buffer(pack_buffer *pb, uint64_t first_byte_num,
                                 const byte *pack) {
    // Reserve space for missing packs, if there are any (if missing > 0).
    uint64_t missing = first_byte_num - pb->head_byte_num;

    uint64_t pack_num;

    if (missing > pb->capacity) {
        // This won't fit. Reset the buffer.
        missing = (pb->capacity / pb->psize - 1) * pb->psize;
        pb_reset(pb, pb->psize, first_byte_num - missing);
        pb->head = pb->head + missing + pb->psize;
        return missing;
    }

    if (pb->head + missing >= pb->buf_end)
        // Buffer overflow.
        pack_num = handle_buffer_overflow(pb, missing, pack);
    else
        // Nothing to worry about.
        pack_num = normal_insert(pb, missing, pack);

    return pack_num;
}

/**
 * Inserts the @p pack into the buffer in case it was missing and increases
 * pack buffer's count, provided it was not a duplicate.
 * @param pb - pointer to pack buffer
 * @param first_byte_num - byte number identifying the pack
 * @param pack - pointer to pack's data
 */
void handle_missing_or_duplicated(pack_buffer *pb, uint64_t
first_byte_num, const byte *pack) {
    uint64_t pack_num = (pb->head + pb->head_byte_num - first_byte_num) -
                        pb->buf;

    if (!pb->is_present[pack_num]) {
        // Found a missing pack.
        pb->is_present[pack_num] = true;
        pb->count++;

        memcpy(pb->head + pb->head_byte_num - first_byte_num, pack,
               pb->psize);
    }
}

void pb_push_back(pack_buffer *pb, uint64_t first_byte_num, const byte *pack,
                  uint64_t psize) {
    if (pb->psize != psize) fatal(psize_errmsg);

    if (pb->head_byte_num > first_byte_num) {
        if (pb->head_byte_num - first_byte_num < pb->capacity) {
            // Encountered a missing or duplicated pack.
            handle_missing_or_duplicated(pb, first_byte_num, pack);
            find_missing(pb, first_byte_num);
            return;
        } else
            // Encountered a missing, but ancient package... ignore.
            return;

    }

    uint64_t pack_num = insert_pack_into_buffer(pb, first_byte_num, pack);

    pb->head_byte_num = first_byte_num + pb->psize;
    pb->is_present[pack_num] = true;
    pb->count++;

    find_missing(pb, first_byte_num);
    handle_buf_end_overlap(&pb->head, pb);
}

void pb_pop_front(pack_buffer *pb, void *item, uint64_t psize) {
    if (pb->psize != psize) fatal(psize_errmsg);

    if (pb->count == 0) {
        // Buffer is depleted. We will wait for it to fill up to approx. 75%
        // to avoid unstable playback.
        pb->byte_zero = pb->head_byte_num;
        return;
    }

    if (pb->head_byte_num - pb->byte_zero < pb->capacity / 4 * 3)
        // Stop playback until (BYTE0 + 3/4 * PSIZE)'th byte received.
        return;

    memcpy(item, pb->tail, psize);
    pb->is_present[pb->tail - pb->buf] = false;
    pb->tail = (byte *) pb->tail + psize;
    pb->count--;

    handle_buf_end_overlap(&pb->tail, pb);
}

