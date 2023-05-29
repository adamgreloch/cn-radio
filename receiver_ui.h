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

/**
 * Initializes empty stations structure
 * @returns pointer to stations struct
 */
stations *init_stations();

/**
 * Adds or updates station information.
 * @param st - pointer to stations struct
 * @param mcast_addr_str - string representation of station's IPv4 address
 * @param port - station's port
 * @param name - station's name
 */
void
st_update(stations *st, char *mcast_addr_str, uint16_t port, char *name);

/**
 * Deletes stations, which information was not updated for longer than @p
 * inactivity_sec.
 * @param st - pointer to stations struct
 * @param inactivity_sec - maximum inactivity time until deletion
 */
void st_delete_inactive_stations(stations *st, uint64_t inactivity_sec);

/**
 * Writes UI to buffer @p buf. If UI won't fit into the buffer, resizes it,
 * updating @p buf_size appropriately.
 * @param buf - pointer to destination buffer
 * @param buf_size - pointer to destination buffer size
 * @param ui_size - size of the UI written
 * @param st - pointer to stations struct
 */
void
st_print_ui(char **buf, uint64_t *buf_size, uint64_t *ui_size, stations *st);

/*
 * Selects cyclically one station higher on the list as current.
 * If there is zero or one station on the list, does nothing. Otherwise,
 * issues a switch.
 */
void st_select_station_up(stations *st);

/*
 * Selects cyclically one station lower on the list as current.
 * If there is zero or one station on the list, does nothing. Otherwise,
 * issues a switch.
 */
void st_select_station_down(stations *st);

/**
 * If there is a switch pending (issued by st_select_station_down/up),
 * updates @p new_station with new station details. Otherwise does nothing.
 * @param st - pointer to stations struct
 * @param new_station - pointer to new station details
 * @returns true if performed a station switch; false otherwise
 */
bool st_switch_if_changed(stations *st, station *new_station);

/**
 * Blocks until any station is added (via st_update()) if no name is
 * prioritized, or until a station with a prioritized name is added if one
 * exists.
 * @param st - pointer to stations struct
 */
void st_wait_until_station_found(stations *st);

/**
 * Marks a name @p station_name as prioritized.
 * @param st - pointer to stations struct
 * @param station_name - char array representing a station name
 */
void st_prioritize_name(stations *st, char *station_name);

/**
 * Bumps last visited timestamp of currently played station. Useful to avoid
 * breaks in playback in case station discoverer fails to rediscover the
 * station in time (i.e. due to package loss) and when it is obvious that the
 * station is active, because we receive audio from it.
 * @param st - pointer to stations struct
 */
void st_bump_current_station(stations *st);

/**
 * UI manager thread function. Handles TCP connections on the UI_PORT,
 * updated UI and listens to interactions.
 * @param args - pointer to stations struct
 */
void *ui_manager(void *args);

/**
 * Station discoverer thread function. Sends LOOKUP messages to DISCOVER_ADDR
 * and updates station list appropriately.
 * @param args - pointer to stations struct
 */
void *station_discoverer(void *args);

#endif //_RECEIVER_UI_
