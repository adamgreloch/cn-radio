#include <stdio.h>
#include <stdlib.h>
#include "receiver_ui.h"

int main() {
    stations *st = init_stations();

    char *mcast_addr_strs[] = {"123.456.789", "123.123.132", "111.111.111"};
    uint16_t ports[] = {1234, 4321, 1111};
    char *names[] = {"Radio Muzyczka", "Radio Polonia", "Radio 123"};

    uint64_t buf_size = 512;
    char *buf = calloc(sizeof(char), buf_size);

    uint64_t ui_size;

    station *new;

    for (int i = 0; i < 3; i++) {
        st_update(st, mcast_addr_strs[i], ports[i], names[i]);

        st_switch_if_changed(st, &new);

        st_print_ui(&buf, &buf_size, &ui_size, st);

        printf("%s", buf);
    }
    for (int i = 0; i < 3; i++) {
        st_update(st, mcast_addr_strs[i], ports[i], names[i]);

        st_switch_if_changed(st, &new);

        st_print_ui(&buf, &buf_size, &ui_size, st);

        printf("%s", buf);
    }


    for (int i = 0; i < 3; i++) {
        st_select_station_down(st);

        st_print_ui(&buf, &buf_size, &ui_size, st);

        st_switch_if_changed(st, &new);

        printf("%s", buf);
    }
}