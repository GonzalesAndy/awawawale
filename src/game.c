#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

void game_init(game_t *g, const char *player_a_name, const char *player_b_name) {
    for (int i = 0; i < N_PITS; ++i) g->pits[i] = SEEDS_PER_PIT;
    g->score[0] = g->score[1] = 0;
    /* seed RNG once for random start */
    srand((unsigned)time(NULL));
    g->turn = (rand() & 1) ? PLAYER_A : PLAYER_B; // random start
}

static bool pit_belongs_to(player_t p, int pit) {
    // who owns which pits
}

bool game_is_over(const game_t *g) {
    // checks if game is over
}

void game_print(const game_t *g) {
    // prints the game state
}

bool game_make_move(game_t *g, player_t p, int pit_index) {
    // makes a move for player p from pit_index
}

int game_save(const game_t *g, const char *path) {
    // saves the game state to a file
}

int game_load(game_t *g, const char *path) {
    // loads the game state from a file
}
