#include "ctrl_protocol.h"
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    char *msg = "BOREWICZ_HERE 123.456.789 4242 Radio Kapitał";
    uint64_t msg_size = strlen(msg);
    char buf[200];
    memcpy(buf, msg, msg_size);
    char mcast_addr_str[20];
    uint16_t port;
    char sender_name[65];

    parse_reply(buf, msg_size, mcast_addr_str, &port, sender_name);

    assert(what_message(msg) == REPLY);
    assert(strcmp(mcast_addr_str, "123.456.789") == 0);
    assert(port == 4242);
    assert(strcmp(sender_name, "Radio Kapitał") == 0);

    msg = "LOUDER_PLEASE 1,2,3,4,5,6,7,8,9,10";
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

    msg = "BOREWICZ_PLEASE 1,2,3,4,5,6,7,8,9,10";
    assert(what_message(msg) == -1);
}