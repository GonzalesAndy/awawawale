#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define N_PITS 12
#define SEEDS_PER_PIT 4

// Limits and sizes used by the in-memory representations
#define GAME_MAX_USERNAME 32
#define GAME_MAX_MOVES 256
#define GAME_MAX_OBSERVERS 16

typedef enum { PLAYER_A = 0, PLAYER_B = 1, TIE = 2 } player_t;

typedef enum {
    GAME_STATE_NEW = 0,
    GAME_STATE_ONGOING,
    GAME_STATE_FINISHED,
    GAME_STATE_ABORTED
} game_state_t;

// Single recorded move in a game's history
typedef struct {
    player_t player;    // who played
    int pit_index;      // 0..11 index of the pit chosen
} game_move_t;

// Representation of an Awalé game in memory. This contains the board,
// scores, player names, history and observer metadata. This file only
// declares the structure and related helper prototypes; behavior is
// implemented elsewhere.
typedef struct {
    uint64_t id;                 // unique game id assigned by server

    int pits[N_PITS];            // 0..5 = player A's pits, 6..11 = player B's pits
    int score[2];                // accumulated captured seeds for players A and B
    player_t turn;               // whose turn it is

    char player_name[2][GAME_MAX_USERNAME]; // human-readable player names (A then B)

    game_state_t state;          // game state

    // Move history (simple fixed-size ring/append log). Only used for
    // saving/reviewing games and for observers.
    game_move_t moves[GAME_MAX_MOVES];
    int moves_len;

    // Observers allowed to watch this game. In private mode only users
    // in the allowed_spectators list may observe.
    bool private_mode;                      // when true, only allowed_spectators may observe
    char allowed_spectators[GAME_MAX_OBSERVERS][GAME_MAX_USERNAME];
    int allowed_spectators_count;
} game_t;

// Initialize a fresh game with default starting seeds. If player names are
// provided (non-NULL) they will be copied into the game metadata.
void game_init(game_t *g, const char *player_a_name, const char *player_b_name);

// Make a move for the given player from pit index. Returns true if move
// was legal and applied; false otherwise. (Behavior implemented elsewhere.)
bool game_make_move(game_t *g, player_t p, int pit_index);

// Check whether move would be legal for the given player without applying it.
bool game_is_move_legal(const game_t *g, player_t p, int pit_index);

// Obtain a text representation of the board into the provided buffer.
// The resulting string is NUL terminated. Returns dest on success.
char *game_to_string(const game_t *g, char *dest, size_t n);

// Print board to stdout (ASCII) or write a compact client-friendly
// representation into the provided destination buffer.
// If dest is NULL or n == 0 the function prints the ASCII board to stdout.
// If dest is non-NULL it writes a single-line representation into dest
// (NUL-terminated) suitable for sending to clients.
void game_print(const game_t *g, char *dest, size_t n);

// Check whether game is over.
bool game_is_over(const game_t *g);

// Save a game's record to a persistent storage (append). Returns 0 on success.
int game_save_record(const game_t *g, const char *path);

// Load a previously saved game record from path into g. Returns 0 on success.
int game_load_record(game_t *g, const char *path);

// Helpers to manage observers and private spectator lists
int game_add_observer(game_t *g, const char *username);
int game_remove_observer(game_t *g, const char *username);
int game_add_allowed_spectator(game_t *g, const char *username);
int game_remove_allowed_spectator(game_t *g, const char *username);

#endif // GAME_H
