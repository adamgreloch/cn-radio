#include "ctrl_protocol.h"
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    char *msg = "BOREWICZ_HERE 233.222.111.111 4242 Radio Kapitał\n";
    uint64_t msg_size = strlen(msg);
    char buf[200];
    memcpy(buf, msg, msg_size);
    char mcast_addr_str[20];
    uint16_t port;
    char sender_name[65];

    parse_reply(buf, msg_size, mcast_addr_str, &port, sender_name);

    assert(what_message(msg) == REPLY);
    assert(strcmp(mcast_addr_str, "233.222.111.111") == 0);
    assert(port == 4242);
    assert(strcmp(sender_name, "Radio Kapitał") == 0);

    msg = "LOUDER_PLEASE 1,2,3,4,5,6,7,8,9,10\n";
    msg_size = strlen(msg);
    memset(buf, 0, 200);
    memcpy(buf, msg, msg_size);
    uint64_t *packs = malloc(420 * sizeof(uint64_t));
    uint64_t n_packs;

    assert(what_message(msg) == REXMIT);
    parse_rexmit(buf, packs, &n_packs);

    assert(n_packs == 10);
    for (int i = 0; i < n_packs; i++)
        assert(packs[i] == i + 1);

    msg = "BOREWICZ_PLEASE 1,2,3,4,5,6,7,8,9,10\n";
    assert(what_message(msg) == -1);

    msg = "LOUDER_PLEASE 0, 32, 16, 0\n";
    msg_size = strlen(msg);
    memset(buf, 0, 200);
    memcpy(buf, msg, msg_size);

    assert(what_message(msg) == REXMIT);
    parse_rexmit(buf, packs, &n_packs);

    assert(n_packs == 4);
    for (int i = 0; i < n_packs; i++)
        printf("%lu\n", packs[i]);
}