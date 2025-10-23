#include "../src/game.h"
#include <stdio.h>

int main(void) {
    game_t g;
    game_init(&g);
    printf("Starting playthrough (turn: %s)\n", g.turn==PLAYER_A?"A":"B");
    game_print(&g);

    // Find a legal pit for current player
    int chosen = -1;
    if (g.turn == PLAYER_A) {
        for (int i = 0; i < 6; ++i) if (g.pits[i] > 0) { chosen = i; break; }
    } else {
        for (int i = 6; i < 12; ++i) if (g.pits[i] > 0) { chosen = i; break; }
    }
    if (chosen == -1) { printf("No legal move\n"); return 1; }
    printf("Player %s plays pit %d\n", g.turn==PLAYER_A?"A":"B", chosen);
    if (!game_make_move(&g, g.turn, chosen)) { printf("Move failed\n"); return 1; }
    game_print(&g);
    return 0;
}
