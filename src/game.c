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

static bool pit_belongs_to(player_t p, int pit) {
    if (p == PLAYER_A) return (pit >= 0 && pit < 6);
    return (pit >= 6 && pit < 12);
}

bool game_is_over(const game_t *g) {
    int sumA = 0, sumB = 0;
    for (int i = 0; i < 6; ++i) sumA += g->pits[i];
    for (int i = 6; i < 12; ++i) sumB += g->pits[i];
    return (sumA == 0 || sumB == 0);
}

void game_print(const game_t *g) {
    printf("Scores: A=%d  B=%d\n", g->score[0], g->score[1]);
    printf("  B  ");
    for (int i = 11; i >= 6; --i) printf(" %2d", g->pits[i]);
    printf("\n");
    printf("     ");
    for (int i = 0; i < 6; ++i) printf(" %2d", g->pits[i]);
    printf("  A\n");
    printf("Turn: %s\n", g->turn==PLAYER_A?"A":"B");
}

bool game_make_move(game_t *g, player_t p, int pit_index) {
    if (p != g->turn) return false; // not player's turn
    if (pit_index < 0 || pit_index >= N_PITS) return false;
    if (!pit_belongs_to(p, pit_index)) return false;
    if (g->pits[pit_index] == 0) return false;

    int seeds = g->pits[pit_index];
    g->pits[pit_index] = 0;
    int idx = pit_index;
    while (seeds > 0) {
        idx = (idx + 1) % N_PITS;
        g->pits[idx] += 1;
        seeds -= 1;
    }

    // capture rule: If last seed lands in opponent's pits and the pit now has 2 or 3 seeds, capture
    while (true) {
        if (pit_belongs_to(p, idx)) break; // landed in player's own pit -> no capture
        if (g->pits[idx] == 2 || g->pits[idx] == 3) {
            int captured = g->pits[idx];
            g->pits[idx] = 0;
            g->score[p==PLAYER_A?0:1] += captured;
            idx = (idx - 1 + N_PITS) % N_PITS; // continue checking previous pit
            continue;
        }
        break;
    }

    // switch turn
    g->turn = (g->turn==PLAYER_A)?PLAYER_B:PLAYER_A;
    return true;
}

int game_save(const game_t *g, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%d %d\n", g->score[0], g->score[1]);
    fprintf(f, "%d\n", g->turn);
    for (int i = 0; i < N_PITS; ++i) fprintf(f, "%d ", g->pits[i]);
    fprintf(f, "\n");
    fclose(f);
    return 0;
}

int game_load(game_t *g, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%d %d\n", &g->score[0], &g->score[1]) != 2) { fclose(f); return -1; }
    int t;
    if (fscanf(f, "%d\n", &t) != 1) { fclose(f); return -1; }
    g->turn = (t==0)?PLAYER_A:PLAYER_B;
    for (int i = 0; i < N_PITS; ++i) {
        if (fscanf(f, "%d", &g->pits[i]) != 1) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}
