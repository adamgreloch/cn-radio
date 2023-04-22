#include "pack_buffer.h"
#include "common.h"

int main() {
    uint64_t psize = 1;
    uint64_t bsize = 10;
    byte *pack = malloc(psize);
    pack[0] = 42;

    pack_buffer *pb = pb_init(bsize);
    pb_reset(pb, psize, 0);

    // Wysyłam najpierw paczki:
    // 0 1 2 3 6 7 8 9
    for (int i = 0; i < 4; i++)
        pb_push_back(pb, i, pack, psize);

    for (int i = 6; i < 10; i++)
        pb_push_back(pb, i, pack, psize);

    // Powinno wykryć, że brakuje paczek:
    // 4 5
}