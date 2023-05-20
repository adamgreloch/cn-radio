#include "receiver_ui.h"
#include "err.h"
#include <pthread.h>
#include <stdlib.h>

char line_break[] = "------------------------------------------------------------------------\r\n";

#define INIT_SIZE 16
#define INIT_UI_BUF_SIZE 4096

struct stations {
    station **data;
    uint64_t count;
    uint64_t size;

    bool change_pending;
    station* current;
    uint64_t current_pos;

    char *ui_buffer;
    uint64_t ui_buffer_size;

    pthread_mutex_t mutex;
    pthread_cond_t wait_for_change;
    pthread_cond_t wait_for_found;
};

stations *init_stations() {
    stations *st = malloc(sizeof(stations));
    st->data = calloc(INIT_SIZE, sizeof(station *));

    st->count = 0;
    st->size = INIT_SIZE;
    st->current = NULL;
    st->current_pos = 0;
    st->change_pending = false;

    st->ui_buffer = calloc(INIT_UI_BUF_SIZE, sizeof(char));
    st->ui_buffer_size = INIT_UI_BUF_SIZE;

    CHECK_ERRNO(pthread_mutex_init(&st->mutex, NULL));
    CHECK_ERRNO(pthread_cond_init(&st->wait_for_change, NULL));
    CHECK_ERRNO(pthread_cond_init(&st->wait_for_found, NULL));

    return st;
}

int _str_compare(char *name1, char* name2) {
    size_t pos = 0;

    while (true) {
        if (name1[pos] == '\0' && name2[pos] == '\0')
            return 0;
        else if (name1[pos] == '\0')
            return -1;
        else if (name2[pos] == '\0')
            return 1;
        else if (name1[pos] == name2[pos])
            pos++;
        else
            return name1[pos] - name2[pos];
    }
}

int _station_compare(const void *a, const void *b) {
    station *st1 = *(station **) a;
    station *st2 = *(station **) b;
    if (!st1 && !st2) return 0;
    if (!st1) return 1;
    if (!st2) return -1;
    return _str_compare(st1->name, st2->name);
}

void _sort_stations(stations *st) {
    qsort(st->data, st->size, sizeof(station*), _station_compare);

    uint64_t i = 0;
    // TODO optimize to binsearch
    while (st->data[i] != st->current)
        i++;
    st->current_pos = i;
}

void
update_station(stations *st, char *mcast_addr_str, uint16_t port, char *name) {
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));

    while (st->change_pending)
        CHECK_ERRNO(pthread_cond_wait(&st->wait_for_change, &st->mutex));

    if (st->count == st->size) {
        st->data = realloc(st->data, 2 * st->size);
        if (!st->data)
            fatal("realloc");
        for (size_t i = 0; i < st->size; i++)
            st->data[i + st->size] = NULL;
        st->size *= 2;
    }

    size_t empty = st->size + 1;
    bool found = false;

    for (size_t i = 0; i < st->size && !found; i++) {
        if (st->data[i] == NULL) {
            if (empty == st->size + 1)
                empty = i;
        }
        // do the equality comparisons from cheapest to most expensive
        else if (st->data[i]->port == port &&
                _str_compare(mcast_addr_str, st->data[i]->mcast_addr) == 0
                && _str_compare(name, st->data[i]->name) == 0) {
            // station rediscovered, just update activity time
            st->data[i]->last_heard = time(NULL);
            found = true;
        }
    }

    if (!found) {
        st->data[empty] = malloc(sizeof(station));
        if (!st->data[empty])
            fatal("malloc");

        station *curr = st->data[empty];
        memcpy(curr->mcast_addr, mcast_addr_str, strlen(mcast_addr_str));
        curr->port = port;
        curr->last_heard = time(NULL);
        memcpy(curr->name, name, strlen(name));
        st->count++;
    }

    if (!st->current) {
        st->current = st->data[0];
        st->current_pos = 0;
        st->change_pending = true;
        CHECK_ERRNO(pthread_cond_broadcast(&st->wait_for_found));
    }

    _sort_stations(st);

    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void _move_selection(stations *st, int delta) {
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    if (st->count > 1) {
        st->current_pos = (st->current_pos + st->count + delta) % st->count;
        st->current = st->data[st->current_pos];
        st->change_pending = true;
    }
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void print_ui(char **buf, uint64_t *buf_size, uint64_t *ui_size, stations *st) {
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    size_t wrote = 0;
    memset(st->ui_buffer, 0, st->ui_buffer_size);
    wrote += sprintf(st->ui_buffer,
                     "\033[H\033[J%s SIK Radio\r\n%s", line_break, line_break);

    for (size_t i = 0; i < st->count; i++) {
        if (wrote + 128 > st->ui_buffer_size) {
            st->ui_buffer_size *= 2;
            st->ui_buffer = realloc(st->ui_buffer, st->ui_buffer_size);
            if (!st->ui_buffer)
                fatal("realloc");
        }
        if (st->data[i] != st->current)
            wrote += sprintf(st->ui_buffer + wrote,
                             " %s\r\n%s", st->data[i]->name, line_break);
        else
            wrote += sprintf(st->ui_buffer + wrote,
                             " > %s\r\n%s", st->data[i]->name, line_break);
    }

    if (st->count == 0)
        wrote += sprintf(st->ui_buffer + wrote,
                         " No stations found\r\n%s", line_break);

    if (*buf_size < wrote) {
        *buf = realloc(*buf, wrote);
        if (!(*buf))
            fatal("realloc");
    }

    memcpy(*buf, st->ui_buffer, wrote);

    *ui_size = wrote;

    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void select_station_up(stations *st) {
    _move_selection(st, -1);
}

void select_station_down(stations *st) {
    _move_selection(st, 1);
}

bool switch_if_changed(stations *st, station **new_station) {
    bool res = false;
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    if (st->change_pending) {
        *new_station = st->current;
        st->change_pending = false;
        CHECK_ERRNO(pthread_cond_signal(&st->wait_for_change));
        res = true;
    }
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
    return res;
}

void delete_inactive_stations(stations *st, uint64_t inactivity_sec) {
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    if (st->count > 0) {
        uint64_t now = time(NULL);
        uint64_t prev_count = st->count;
        for (size_t i = 0; i < prev_count; i++)
            if (now - st->data[i]->last_heard >= inactivity_sec) {
                if (st->data[i] == st->current)
                    st->current = NULL;
                free(st->data[i]);
                st->data[i] = NULL;
                st->count--;
            }

        _sort_stations(st);
    }
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void remove_current_for_inactivity(stations *st) {
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    if (!st->change_pending && st->count > 0) {
        free(st->current);
        st->current = NULL;
        st->data[st->current_pos] = NULL;
        st->count--;
        _sort_stations(st);
    }
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void wait_until_any_station_found(stations *st) {
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    while (!st->current)
        CHECK_ERRNO(pthread_cond_wait(&st->wait_for_found, &st->mutex));
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}