#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "err.h"
#include "common.h"

#define BUFFER_SIZE 20000

byte *pack_buffer;
byte *is_filled;
uint64_t buffer_oldest = 0;
uint64_t buffer_newest = 0;
uint64_t next_to_read = 0;
pthread_mutex_t mutex;
pthread_cond_t missing_next;
pthread_cond_t wait_for_fill;

uint16_t port = DEFAULT_PORT;       // UDP data port
uint64_t bsize = DEFAULT_BSIZE;     // buffer size
uint64_t psize = DEFAULT_PSIZE;

void pack_buffer_reset() {
    memset(pack_buffer, 0, bsize);
    buffer_oldest = 0;
    buffer_newest = 0;
    next_to_read = 0;
}

void insert_pack(uint64_t first_byte_num, byte *data) {
    CHECK_ERRNO(pthread_mutex_lock(&mutex));
    uint64_t pos = buffer_oldest;

    while (first_byte_num - buffer_oldest > bsize) {
        // oldest is too old
        // remove it from buffer and bump one step up
        pos = buffer_oldest % bsize;

        if (pack_buffer[pos % bsize] != 0) {
            memset(pack_buffer + (pos % bsize), 0, psize);
            is_filled[(pos % bsize) / psize] = 0;
        }

        buffer_oldest += psize;
    }

    while (pos < first_byte_num) {
        if (is_filled[(pos % bsize) / psize] == 0) {
            fprintf(stderr, "MISSING: BEFORE %lu EXPECTED %lu\n",
                    first_byte_num, pos);
        }
        pos = pos + psize;
    }

    memcpy(pack_buffer + (first_byte_num % bsize), data, psize);
    is_filled[(first_byte_num % bsize) / psize] = 1;

    if (first_byte_num > buffer_newest) {
        buffer_newest = first_byte_num;
        if (buffer_newest >= bsize / 4 * 3)
            CHECK_ERRNO(pthread_cond_signal(&wait_for_fill));
        else
            fprintf(stderr, "WAITING FOR BUFFER TO FILL (%f)\n",
                    (float) buffer_newest / (float) (bsize / 4 * 3));
    }

    if (first_byte_num == next_to_read)
        CHECK_ERRNO(pthread_cond_signal(&missing_next));

    CHECK_ERRNO(pthread_mutex_unlock(&mutex));
}

void pop_next(byte *buf) {
    CHECK_ERRNO(pthread_mutex_lock(&mutex));

    while (buffer_newest < bsize / 4 * 3)
        CHECK_ERRNO(pthread_cond_wait(&wait_for_fill, &mutex));

    while (is_filled[(next_to_read % bsize) / psize] == 0)
        CHECK_ERRNO(pthread_cond_wait(&missing_next, &mutex));

    memcpy(buf, pack_buffer + (next_to_read % bsize), psize);
    memset(pack_buffer + (next_to_read % bsize), 0, psize);
    is_filled[(next_to_read % bsize) / psize] = 0;

    if (next_to_read == buffer_oldest)
        buffer_oldest += psize;

    next_to_read += psize;

    CHECK_ERRNO(pthread_mutex_unlock(&mutex));
}

void *gather_packs() {
    byte *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
        fatal("malloc");

    is_filled = malloc(BUFFER_SIZE); // TODO temp

    int socket_fd = bind_socket(port);

    struct sockaddr_in client_address;

    size_t read_length;
    struct audio_pack pack;

    uint64_t curr_session_id;
    uint64_t last_session_id = 0;

    do {
        memset(buffer, 0, BUFFER_SIZE);

        socklen_t address_length = (socklen_t) sizeof(client_address);

        int flags = 0;

        errno = 0;
        ENSURE(sizeof(struct audio_pack) == recvfrom(socket_fd, &pack, sizeof
                                                             (struct audio_pack), flags, (struct sockaddr *) &client_address,
                                                     &address_length));

        memset(buffer, 0, BUFFER_SIZE);

        curr_session_id = htonll(pack.session_id);
        if (curr_session_id > last_session_id) {
            pack_buffer_reset();
        }

        last_session_id = curr_session_id;

        read_length = recvfrom(socket_fd, buffer, BUFFER_SIZE,
                               flags, (struct sockaddr *) &client_address,
                               &address_length);

        psize = read_length;

        pack.audio_data = (byte *) buffer;

        if (read_length < 0) {
            PRINT_ERRNO();
        }

        fprintf(stderr, "\nfirst_byte_num=%lu\n", ntohll(pack.first_byte_num));
        fprintf(stderr, "psize=%u\n", psize);
        fprintf(stderr, "session_id=%lu\n", ntohll(pack.session_id));

        char *client_ip = inet_ntoa(client_address.sin_addr);
        uint16_t client_port = ntohs(client_address.sin_port);

        fprintf(stderr, "received %zd bytes from client %s:%u\n",
                read_length, client_ip, client_port);

        insert_pack(ntohll(pack.first_byte_num), pack.audio_data);

    } while (read_length > 0);

    CHECK_ERRNO(close(socket_fd));

    return 0;
}

void *print_packs() {
    byte *write_buffer = calloc(BUFFER_SIZE, 1);
    if (!write_buffer)
        fatal("calloc");

    while (true) {
        pop_next(write_buffer);
        fwrite(write_buffer, psize, sizeof(byte), stdout);
    }

    return 0;
}

int main() {
    pack_buffer = malloc(bsize);
    if (!pack_buffer)
        fatal("malloc");

    memset(pack_buffer, 0, bsize);

    CHECK_ERRNO(pthread_mutex_init(&mutex, NULL));
    CHECK_ERRNO(pthread_cond_init(&missing_next, NULL));

    pthread_t gatherer_id;
    pthread_t printer_id;

    CHECK_ERRNO(pthread_create(&gatherer_id, NULL, gather_packs, NULL));
    CHECK_ERRNO(pthread_create(&printer_id, NULL, print_packs, NULL));

    CHECK_ERRNO(pthread_join(gatherer_id, NULL));
    CHECK_ERRNO(pthread_join(printer_id, NULL));

    CHECK_ERRNO(pthread_mutex_destroy(&mutex));
    free(pack_buffer);

    return 0;
}
