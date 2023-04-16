#include "pack_buffer_seq.h"

typedef struct pack_buffer {
    byte *buffer;     // data buffer
    byte *buffer_end; // end of data buffer

    bool *is_present; // indicates whether i-th pack is present or missing

    size_t capacity;  // maximum number of items in the buffer
    uint64_t psize;
    size_t count;     // number of items in the buffer
    byte *head;       // pointer to head
    uint64_t head_byte_num;
    uint64_t byte_zero;
    byte *tail;       // pointer to tail
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
    pb->head_byte_num = pb->byte_zero = 0;
    return pb;
}

void pb_free(pack_buffer *pb) {
    free(pb->buffer);
    free(pb->is_present);
    free(pb);
}

void pb_reset(pack_buffer *pb, uint64_t psize, uint64_t byte_zero) {
    memset(pb->buffer, 0, pb->capacity);
    memset(pb->is_present, 0, pb->capacity);

    pb->buffer_end = (byte *) pb->buffer + pb->capacity;
    pb->psize = psize;
    pb->count = 0;

    pb->head_byte_num = pb->byte_zero = byte_zero;
    pb->head = pb->tail = pb->buffer;
}

void print_missing(pack_buffer *pb, uint64_t first_byte_num) {
    // TODO check whether this really works:
    byte *pos = pb->tail;
    uint64_t missing_byte_num;
    while (pos != pb->head) {
        if (!pb->is_present[pos - pb->buffer]) {
            if (pos < pb->head)
                missing_byte_num = pb->head_byte_num - (pb->head - pos);
            else
                missing_byte_num = pb->head_byte_num
                                   - (pb->head - pb->buffer + pb->buffer_end -
                                      pos);

            fprintf(stderr, "MISSING: BEFORE %lu EXPECTED %lu\n",
                    first_byte_num, missing_byte_num);
        }
        pos += pb->psize;

        // TODO handle overlap when bsize isnt divisible by psize
        if (pos == pb->buffer_end)
            pos = pb->buffer;
    }
}

uint64_t insert_pack_into_buffer(pack_buffer *pb, uint64_t first_byte_num,
                                 const byte *pack) {
    // reserve space for missing packs if there are any (if missing > 0)
    uint64_t missing = first_byte_num - pb->head_byte_num;

    uint64_t pack_num;

    if (missing > pb->capacity) {
        // this won't fit. reset the buffer
        missing = (pb->capacity / pb->psize - 1) * pb->psize;
        pb_reset(pb, pb->psize, first_byte_num - missing);
        pb->head = pb->head + missing + pb->psize;
        return missing;
    }

    if (pb->head + missing >= pb->buffer_end) {
        // buffer overflow
        uint64_t before_overflow_count = pb->buffer_end - pb->head;

        memset(pb->head, 0, before_overflow_count);
        memset(pb->buffer + missing - before_overflow_count, 0, pb->psize);
        memcpy(pb->buffer + missing - before_overflow_count + pb->psize, pack,
               pb->psize);

        pb->head = pb->buffer + missing - before_overflow_count + 2 * pb->psize;

        // TODO fix overlapping -- segfaults
//        if (pb->buffer + (missing % pb->capacity) + pb->psize >= pb->tail) {
//            // head overlapped the tail
//            memset(pb->tail, 0, pb->head - pb->tail);
//            pb->tail = pb->head + pb->psize;
//
//            if (pb->tail == pb->buffer_end)
//                pb->tail = pb->buffer;
//        }
        pack_num = pb->head - pb->buffer - pb->psize;
    } else {
        memcpy(pb->head + missing, pack, pb->psize);

        pb->head = (byte *) pb->head + missing + pb->psize;
        pack_num = pb->head - pb->buffer - pb->psize;

        if (pb->head == pb->buffer_end)
            pb->head = pb->buffer;
    }

    return pack_num;
}

void pb_push_back(pack_buffer *pb, uint64_t first_byte_num, const byte *pack,
                  uint64_t psize) {
    if (pb->psize != psize) {}

    if (pb->head_byte_num > first_byte_num) {
        // encountered a missing or duplicated pack
        memcpy(pb->head + pb->head_byte_num - first_byte_num, pack, psize);

        uint64_t pack_num = (pb->head + pb->head_byte_num - first_byte_num) -
                            pb->buffer;

        if (!pb->is_present[pack_num]) {
            // found missing pack
            pb->is_present[pack_num] = true;
            pb->count++;
        }

        print_missing(pb, first_byte_num);
        return;
    }

    uint64_t pack_num = insert_pack_into_buffer(pb, first_byte_num, pack);

    pb->head_byte_num = first_byte_num + psize;
    pb->is_present[pack_num] = true;
    pb->count++;

    print_missing(pb, first_byte_num);
}

bool pb_pop_front(pack_buffer *pb, void *item, uint64_t psize) {
    if (pb->psize != psize) {}

    if (pb->count == 0)
        return false;

    if (pb->head_byte_num - pb->byte_zero < pb->capacity / 4 * 3)
        return false;

    memcpy(item, pb->tail, psize);
    pb->is_present[pb->tail - pb->buffer] = false;
    pb->tail = (byte *) pb->tail + psize;

    if (pb->tail == pb->buffer_end)
        pb->tail = pb->buffer;

    pb->count--;

    return true;
}

