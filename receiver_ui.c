#include <pthread.h>
#include <stdlib.h>
#include <poll.h>
#include "receiver_ui.h"
#include "receiver_utils.h"
#include "err.h"
#include "ctrl_protocol.h"

char line_break[] = "------------------------------------------------------------------------\r\n";

#define INIT_SIZE 16
#define INIT_UI_BUF_SIZE 4096

struct stations {
    station **data;
    uint64_t count;
    uint64_t size;

    bool change_pending;
    station *current;
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

int _str_compare(char *name1, char *name2) {
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
    qsort(st->data, st->size, sizeof(station *), _station_compare);

    uint64_t i = 0;
    if (st->current) {
        while (st->data[i] != st->current)
            i++;
        st->current_pos = i;
    }
}

void
update_station(stations *st, char *mcast_addr_str, uint16_t port, char *name) {
    if (!st) fatal("null argument");
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

    _sort_stations(st);

    if (!st->current) {
        st->current = st->data[0];
        st->current_pos = 0;
        st->change_pending = true;
        fprintf(stderr, "set current to %s\n", st->current->mcast_addr);
        CHECK_ERRNO(pthread_cond_broadcast(&st->wait_for_found));
    }

    fprintf(stderr, "station %s updated\n", mcast_addr_str);

    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void _move_selection(stations *st, int delta) {
    if (!st) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    if (st->count > 1) {
        st->current_pos = (st->current_pos + st->count + delta) % st->count;
        st->current = st->data[st->current_pos];
        st->change_pending = true;
    }
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void print_ui(char **buf, uint64_t *buf_size, uint64_t *ui_size, stations *st) {
    if (!st) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    size_t wrote = 0;
    memset(st->ui_buffer, 0, st->ui_buffer_size);
    wrote += sprintf(st->ui_buffer,
                     "\033[H\033[J%s SIK Radio\r\n%s", line_break, line_break);

    for (size_t i = 0; i < st->count; i++) {
        if (wrote + MAX_NAME_LEN + 16 > st->ui_buffer_size) {
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

bool switch_if_changed(stations *st, station *new_station) {
    if (!st) fatal("null argument");
    bool res = false;
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));

    while (!st->current) {
        fprintf(stderr, "wait switch if changed\n");
        CHECK_ERRNO(pthread_cond_wait(&st->wait_for_found, &st->mutex));
        fprintf(stderr, "wake switch if changed\n");
    }

    if (st->change_pending) {
        *new_station = *st->current;
        st->change_pending = false;
        CHECK_ERRNO(pthread_cond_broadcast(&st->wait_for_change));
        res = true;
    }
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
    return res;
}

void delete_inactive_stations(stations *st, uint64_t inactivity_sec) {
    if (!st) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    while (st->change_pending) {
        fprintf(stderr, "wait for change inactive\n");
        CHECK_ERRNO(pthread_cond_wait(&st->wait_for_change, &st->mutex));
        fprintf(stderr, "wake for change inactive\n");
    }

    if (st->count > 0) {
        uint64_t now = time(NULL);
        uint64_t prev_count = st->count;
        for (size_t i = 0; i < prev_count; i++)
            if (now - st->data[i]->last_heard >=
                inactivity_sec) {
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

void wait_until_any_station_found(stations *st) {
    if (!st) fatal("null argument");
    CHECK_ERRNO(pthread_mutex_lock(&st->mutex));
    while (!st->current)
        CHECK_ERRNO(pthread_cond_wait(&st->wait_for_found, &st->mutex));
    CHECK_ERRNO(pthread_mutex_unlock(&st->mutex));
}

void *ui_manager(void *args) {
    receiver_data *rd = args;

    int pd_size = INIT_PD_SIZE;

    struct pollfd *pd = calloc(pd_size, sizeof(struct pollfd));
    bool *negotiated = calloc(pd_size, sizeof(bool));

    // initialize client socket table, pd[0] is a central socket
    for (int i = 0; i < pd_size; i++) {
        pd[i].fd = -1;
        pd[i].events = POLLIN | POLLOUT;
        pd[i].revents = 0;
    }

    pd[0].fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (pd[0].fd < 0) {
        PRINT_ERRNO();
    }

    uint64_t ui_buffer_size = UI_BUF_SIZE;
    char *ui_buffer = malloc(sizeof(UI_BUF_SIZE) * ui_buffer_size);
    if (!ui_buffer)
        fatal("malloc");

    char *nav_buffer = malloc(NAV_BUF_SIZE);
    if (!nav_buffer)
        fatal("malloc");

    bind_socket(pd[0].fd, rd->ui_port);

    start_listening(pd[0].fd, QUEUE_LENGTH);

    while (true) {
        for (int i = 0; i < pd_size; i++) {
            pd[i].revents = 0;
        }

        sleep(1);
        int poll_status = poll(pd, pd_size, TIMEOUT);
        if (poll_status == -1)
            PRINT_ERRNO();
        else if (poll_status > 0) {
            if (pd[0].revents & POLLIN) {
                // Accept new connection
                int client_fd = accept_connection(pd[0].fd, NULL);

                bool accepted = false;
                for (int i = 1; i < pd_size; i++) {
                    if (pd[i].fd == -1) {
                        pd[i].fd = client_fd;
                        accepted = true;
                        break;
                    }
                }

                if (!accepted) {
                    // pd table is too small. Resize it
                    pd_size *= 2;
                    pd = realloc(pd, pd_size);
                    if (!pd)
                        fatal("realloc");
                    pd[pd_size].fd = client_fd;
                }
            }

            for (int i = 1; i < pd_size; i++) {
                if (pd[i].fd != -1 &&
                    (pd[i].revents & (POLLIN | POLLERR))) {
                    ssize_t received_bytes = read(pd[i].fd, nav_buffer,
                                                  NAV_BUF_SIZE);
                    if (received_bytes < 0) {
                        // Error when reading message from connection
                        pd[i].fd = -1;
                    } else if (received_bytes == 0) {
                        // Client has closed connection
                        CHECK_ERRNO(close(pd[i].fd));
                        pd[i].fd = -1;
                    } else
                        handle_input(nav_buffer, rd->st);
                }
                if (pd[i].fd != -1 &&
                    (pd[i].revents & (POLLOUT))) {
                    ssize_t sent;
                    memset(ui_buffer, 0, UI_BUF_SIZE);
                    if (!negotiated[i]) {
                        negotiate_telnet(pd[i].fd, ui_buffer);
                        negotiated[i] = true;
                    } else {
                        size_t ui_len;
                        print_ui(&ui_buffer, &ui_buffer_size, &ui_len, rd->st);
                        sent = write(pd[i].fd, ui_buffer, ui_len);
                        ENSURE(sent == (ssize_t) ui_len);
                    }
                }
            }
        }
    }
}

void *station_discoverer(void *args) {
    receiver_data *rd = args;

    int ctrl_sock_fd = create_socket(rd->ctrl_port);
    enable_broadcast(ctrl_sock_fd);

    char *write_buffer = malloc(CTRL_BUF_SIZE);
    if (!write_buffer)
        fatal("malloc");

    int wrote_size;
    ssize_t sent_size;
    ssize_t recv_size;
    int flags = 0;
    errno = 0;

    socklen_t discover_addr_len = (socklen_t) sizeof(rd->discover_addr);

    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = (socklen_t) sizeof(sender_addr);

    char mcast_addr_str[20];
    uint16_t sender_port;
    char sender_name[64 + 1];

    while (true) {
        memset(write_buffer, 0, CTRL_BUF_SIZE);

        wrote_size = write_lookup(write_buffer);
        sent_size = sendto(ctrl_sock_fd, write_buffer, wrote_size,
                           flags, (struct sockaddr *)
                                   &rd->discover_addr, discover_addr_len);
        ENSURE(sent_size == wrote_size);

        sleep(DISCOVER_SLEEP);

        memset(write_buffer, 0, CTRL_BUF_SIZE);

        while ((recv_size = recvfrom(ctrl_sock_fd, write_buffer, CTRL_BUF_SIZE,
                                     MSG_DONTWAIT,
                                     (struct sockaddr *) &sender_addr,
                                     &sender_addr_len)) > 0) {
            if (what_message(write_buffer) == REPLY) {
                parse_reply(write_buffer, recv_size, mcast_addr_str,
                            &sender_port,
                            sender_name);
                update_station(rd->st, mcast_addr_str, sender_port,
                               sender_name);
            }

            memset(mcast_addr_str, 0, sizeof(mcast_addr_str));
            memset(sender_name, 0, sizeof(sender_name));
            memset(write_buffer, 0, CTRL_BUF_SIZE);
        }

        delete_inactive_stations(rd->st, INACTIVITY_THRESH);
    }
}

