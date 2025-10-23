#include "../src/game.h"
#include <stdio.h>

int main(void) {
    game_t g;
    game_init(&g);
    printf("Initial board:\n");
    game_print(&g);

    // Try a move from player A pit 2
    int pit = 2;
    player_t p = g.turn;
    printf("Player %s moves pit %d\n", p==PLAYER_A?"A":"B", pit);
    if (game_make_move(&g, p, pit)) {
        printf("After move:\n");
        game_print(&g);
    } else {
        printf("Move illegal\n");
    }

    return 0;
}
