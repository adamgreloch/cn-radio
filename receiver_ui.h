#ifndef _RECEIVER_UI_
#define _RECEIVER_UI_

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "receiver_config.h"

struct stations;
typedef struct stations stations;

struct station {
    char name[MAX_NAME_LEN + 1];
    char mcast_addr[20];
    uint16_t port;
    uint64_t last_heard;
};
typedef struct station station;

stations *init_stations();

void
st_update(stations *st, char *mcast_addr_str, uint16_t port, char *name);

void st_delete_inactive_stations(stations *st, uint64_t inactivity_sec);

/**
 * Writes UI to buffer @p buf. If UI won't fit into the buffer, resizes it.
 * @param buf
 * @param ui_size
 * @param st
 */
void st_print_ui(char **buf, uint64_t *buf_size, uint64_t *ui_size, stations *st);

void st_select_station_up(stations *st);

void st_select_station_down(stations *st);

bool st_switch_if_changed(stations *st, station *new_station);

void st_wait_until_station_found(stations *st);

void st_prioritize_name(stations *st, char* station_name);

void *ui_manager(void *args);

void *station_discoverer(void *args);

#endif //_RECEIVER_UI_
