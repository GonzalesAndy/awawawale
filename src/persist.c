#include "persist.h"
#include <stdio.h>

int persist_append_game(const game_t *g, const char *logpath) {
    FILE *f = fopen(logpath, "a");
    if (!f) return -1;
    fprintf(f, "Game record:\n");
    fprintf(f, "Scores: %d %d\n", g->score[0], g->score[1]);
    for (int i = 0; i < N_PITS; ++i) fprintf(f, "%d ", g->pits[i]);
    fprintf(f, "\nTurn: %d\n\n", g->turn);
    fclose(f);
    return 0;
}
