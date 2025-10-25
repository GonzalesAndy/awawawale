#include "../src/game.h"
#include "../src/persist.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    game_t g;
    char buf[512];
    const char *logpath = "../tests/game_log.txt";

    /* remove previous log if any */
    remove(logpath);

    game_init(&g, "Alice", "Bob");
    printf("Initial board:\n");
    game_print(&g);
    if (persist_append_game(&g, logpath) != 0) {
        printf("Warning: could not append initial state to %s\n", logpath);
    }

    /* perform 3 moves, saving after each move */
    for (int move = 0; move < 3; ++move) {
        int chosen = -1;
        if (g.turn == PLAYER_A) {
            for (int i = 0; i < N_PITS/2; ++i) if (g.pits[i] > 0) { chosen = i; break; }
        } else {
            for (int i = N_PITS/2; i < N_PITS; ++i) if (g.pits[i] > 0) { chosen = i; break; }
        }
        if (chosen == -1) {
            printf("No legal move for player %s on move %d\n", g.turn==PLAYER_A?"A":"B", move);
            break;
        }
        printf("Move %d: Player %s plays pit %d\n", move+1, g.turn==PLAYER_A?"A":"B", chosen);
        if (!game_make_move(&g, g.turn, chosen)) {
            printf("Move %d failed (illegal)\n", move+1);
            break;
        }
        printf("After move %d:\n", move+1);
        game_print(&g);

        if (!game_to_string(&g, buf, sizeof buf)) {
            printf("game_to_string failed (buffer too small?)\n");
        } else {
            printf("Serialized: %s", buf);
        }

        if (persist_append_game(&g, logpath) == 0) {
            printf("Appended state after move %d to %s\n", move+1, logpath);
        } else {
            printf("Failed to append state to %s\n", logpath);
        }
    }

    printf("Test finished. Log written to %s (if append succeeded).\n", logpath);
    return 0;
}
