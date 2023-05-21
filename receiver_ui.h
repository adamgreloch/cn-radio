#ifndef _RECEIVER_UI_
#define _RECEIVER_UI_

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

struct stations;
typedef struct stations stations;

struct station {
    char name[64 + 1];
    char mcast_addr[20];
    uint16_t port;
    uint64_t last_heard;
};
typedef struct station station;

stations *init_stations();

void
update_station(stations *st, char *mcast_addr_str, uint16_t port, char *name);

void delete_inactive_stations(stations *st, uint64_t inactivity_sec);

/**
 * Writes UI to buffer @p buf. If UI won't fit into the buffer, resizes it.
 * @param buf
 * @param ui_size
 * @param st
 */
void print_ui(char **buf, uint64_t *buf_size, uint64_t *ui_size, stations *st);

void select_station_up(stations *st);

void select_station_down(stations *st);

bool switch_if_changed(stations *st, station **new_station);

void remove_current_for_inactivity(stations *st);

void wait_until_any_station_found(stations *st);

void *ui_manager(void *args);

void *station_discoverer(void *args);

#endif //_RECEIVER_UI_
