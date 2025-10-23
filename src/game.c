#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

void game_init(game_t *g) {
    for (int i = 0; i < N_PITS; ++i) g->pits[i] = SEEDS_PER_PIT;
    g->score[0] = g->score[1] = 0;
    /* seed RNG once for random start */
    srand((unsigned)time(NULL));
    g->turn = (rand() & 1) ? PLAYER_A : PLAYER_B; // random start
}



bool game_is_over(const game_t *g) {
    if (!g) return true;
    /* majority captured */
    int total = N_PITS * SEEDS_PER_PIT;
    if (g->score[0] > total/2 || g->score[1] > total/2) return true;
    /* one side has no seeds */
    int sumA = 0, sumB = 0;
    for (int i = 0; i < N_PITS/2; ++i) sumA += g->pits[i];
    for (int i = N_PITS/2; i < N_PITS; ++i) sumB += g->pits[i];
    if (sumA == 0 || sumB == 0) return true;
    return false;
}

void game_print(const game_t *g) {
    // prints the game state
        printf("                 Cases\n");
    printf("             11    10    9    8    7    6\n");
    printf("           ┌────┬────┬────┬────┬────┬────┐\n");
    printf(" Player B  |");
    for (int i = N_PITS - 1; i >= N_PITS/2; i--) printf(" %2d │", g->pits[i]);
    
    printf("  ← sens de jeu\n");
    printf("           ├────┼────┼────┼────┼────┼────┤\n");
    printf(" → sens de │");
    for (int i = 0; i < N_PITS/2; i++) printf(" %2d │", g->pits[i]);
    printf(" Player A \n");
    printf("     jeu   └────┴────┴────┴────┴────┴────┘\n");
    printf("              0    1    2    3    4    5\n");
}

bool game_make_move(game_t *g, player_t p, int pit_index) 
{
    if (!g) return false;
    if (p != g->turn) return false; /* wrong player's turn */
    //ifplayer A turn
    if (p == PLAYER_A) {
        if (pit_index < 0 || pit_index >= N_PITS/2) return false; // out of bounds
        if (g->pits[pit_index] == 0) return false; /* empty pit */
        
    } else {
        if (pit_index < N_PITS/2 || pit_index >= N_PITS) return false; // out of bounds
        if (g->pits[pit_index] == 0) return false; /* empty pit */
    }
    int seeds = g->pits[pit_index];
        g->pits[pit_index] = 0;
        int index = pit_index;
        while (seeds > 0) {
            index = (index + 1) % N_PITS;
            g->pits[index]++;
            seeds--;//ok
        }
   
    // capture logic
    if (p == PLAYER_A && index >= N_PITS/2) {
        if (g->pits[index] == 2 || g->pits[index] == 3) {
            g->score[0] += g->pits[index];
            g->pits[index] = 0;
        }
    } else if (p == PLAYER_B && index < N_PITS/2) {
        if (g->pits[index] == 2 || g->pits[index] == 3) {
            g->score[1] += g->pits[index];
            g->pits[index] = 0;
        }
    }
    return true;
    
}

int game_save(const game_t *g, const char *path) {
    // saves the game state to a file
}

int game_load(game_t *g, const char *path) {
    // loads the game state from a file
}
