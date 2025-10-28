#include "../src/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    game_t g;
    game_init(&g, "Alice", "Bob");
    printf("Starting playthrough (turn: %s)\n", g.turn==PLAYER_A?"A":"B");
    char buf[1024];
    if (game_print(&g, buf, sizeof buf) >= 0) fputs(buf, stdout);

    char line[128];
    int move_count = 0;

    while (!game_is_over(&g)) {
    printf("\nTurn %d - Player %s\n", move_count+1, g.turn==PLAYER_A?"A":"B");
    if (game_print(&g, buf, sizeof buf) >= 0) fputs(buf, stdout);
        printf("Enter pit index to play (or 'q' to quit): ");
        if (!fgets(line, sizeof line, stdin)) {
            printf("Input error or EOF, exiting.\n");
            break;
        }
        /* trim leading spaces */
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == 'q' || *s == 'Q') {
            printf("Quitting playthrough.\n");
            break;
        }
        /* parse integer */
        char *endptr;
        long val = strtol(s, &endptr, 10);
        if (s == endptr) {
            printf("Invalid input, please enter a number or 'q'.\n");
            continue;
        }
        int chosen = (int)val;

        if (!game_is_move_legal(&g, g.turn, chosen)) {
            printf("Move %d is illegal for player %s. Try again.\n", chosen, g.turn==PLAYER_A?"A":"B");
            continue;
        }

        if (!game_make_move(&g, g.turn, chosen)) {
            printf("Failed to apply move.\n");
            continue;
        }
        move_count++;
    }

    if (game_is_over(&g)) {
        printf("\nGame finished after %d moves. Scores: A=%d B=%d\n", move_count, g.score[0], g.score[1]);
    }

    return 0;
}
