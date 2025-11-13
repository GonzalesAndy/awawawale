#ifndef PERSIST_H
#define PERSIST_H

#include "game.h"

// Persistence API for saving and loading game records and user data.

// Save a game's record to persistent storage. Returns 0 on success.
// Only works for finished games (GAME_STATE_FINISHED).
int game_save_record(const game_t *g, const char *path);

// Load a previously saved game record from path into g. Returns 0 on success.
int game_load_record(game_t *g, const char *path);

#endif // PERSIST_H
