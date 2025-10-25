#include "persist.h"
#include <stdio.h>

int persist_append_game(const game_t *g, const char *logpath) {
    if (!g || !logpath) return -1;
    char buf[512];
    if (!game_to_string(g, buf, sizeof buf)) return -1;
    /* append the single-line serialized state */
    return game_save_record(buf, logpath);
}
