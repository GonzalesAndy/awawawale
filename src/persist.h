#ifndef PERSIST_H
#define PERSIST_H

#include "game.h"

// Save a completed game record (append) to a log file
int persist_append_game(const game_t *g, const char *logpath);

#endif // PERSIST_H
