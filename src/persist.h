#ifndef PERSIST_H
#define PERSIST_H

#include "game.h"

// Persistence API for saving and loading game records and user data.

// Save a completed game record (append) to a log file. Returns 0 on success.
int persist_append_game(const game_t *g, const char *logpath);

// Save a game's record to persistent storage. Returns 0 on success.
// Only works for finished games (GAME_STATE_FINISHED).
int game_save_record(const game_t *g, const char *path);

// Load a previously saved game record from path into g. Returns 0 on success.
int game_load_record(game_t *g, const char *path);

// List saved game ids from a replay store (up to max entries). Returns number found.
int persist_list_saved_games(const char *logpath, uint64_t *out_ids, int max);

// Load a saved game by id from the store into g. Returns 0 on success.
int persist_load_game_by_id(game_t *g, const char *logpath, uint64_t id);

// Save and load user bio/profile data
int persist_save_user_bio(const char *storepath, const char *username, const char *bio_text);
int persist_load_user_bio(const char *storepath, const char *username, char *out_bio, size_t n);

#endif // PERSIST_H
