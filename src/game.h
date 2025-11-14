#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define N_PITS 12
#define SEEDS_PER_PIT 4

#define GAME_MAX_USERNAME 32
#define GAME_MAX_MOVES 256

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
    int pit_index;      // 0..11 index of the pit
} game_move_t;

// Representation of an Awalé game instance
typedef struct {
    uint64_t id;                 // unique game id

    int pits[N_PITS];            // 0..5 = player A pits, 6..11 = player B pits
    int score[2];                // score of players A and B
    player_t turn;               // whose turn it is

    char player_name[2][GAME_MAX_USERNAME]; // player real usernames

    game_state_t state;          // game state

    // Move history
    game_move_t moves[GAME_MAX_MOVES];
    int moves_len;

    bool private_mode;           // when true, only friends may observe
} game_t;

// Initialize the game structure
void game_init(game_t *g, const char *player_a_name, const char *player_b_name);

// Make a move for the given player from pit index. Returns true on success.
bool game_make_move(game_t *g, player_t p, int pit_index);

// Check move legality
bool game_is_move_legal(const game_t *g, player_t p, int pit_index);

// Convert game state to a string representation
char *game_to_string(const game_t *g, char *dest, size_t n);

// Check if game is over.
bool game_is_over(const game_t *g);

#endif // GAME_H
