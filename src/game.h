#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

#define N_PITS 12
#define SEEDS_PER_PIT 4

typedef enum { PLAYER_A = 0, PLAYER_B = 1 } player_t;

typedef struct {
    int pits[N_PITS]; // 0..5 = player A's pits, 6..11 = player B's pits
    int score[2];
    player_t turn; // whose turn it is
} game_t;

// Initialize a game with standard starting seeds
void game_init(game_t *g);

// Make a move for the given player from pit index.
// Returns true if move was legal and applied; false otherwise.
bool game_make_move(game_t *g, player_t p, int pit_index);

// Print board to stdout (ASCII)
void game_print(const game_t *g);

// Check whether game is over.
bool game_is_over(const game_t *g);

// Save game to file path. Returns 0 on success.
int game_save(const game_t *g, const char *path);

// Load game from file path. Returns 0 on success.
int game_load(game_t *g, const char *path);

#endif // GAME_H
