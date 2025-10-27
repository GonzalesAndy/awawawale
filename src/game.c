#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

void game_init(game_t *g, const char *player_a_name, const char *player_b_name) {
    if (!g) return;
    /* initialize board and scores */
    for (int i = 0; i < N_PITS; ++i) g->pits[i] = SEEDS_PER_PIT;
    g->score[0] = g->score[1] = 0;

    g->id = 0;
    g->state = GAME_STATE_NEW;
    g->moves_len = 0;
    g->private_mode = false;
    g->allowed_spectators_count = 0;
    for (int i = 0; i < GAME_MAX_OBSERVERS; ++i) g->allowed_spectators[i][0] = '\0';
    g->player_name[0][0] = '\0';
    g->player_name[1][0] = '\0';

    if (player_a_name) strncpy(g->player_name[0], player_a_name, GAME_MAX_USERNAME - 1);
    if (player_b_name) strncpy(g->player_name[1], player_b_name, GAME_MAX_USERNAME - 1);
    g->player_name[0][GAME_MAX_USERNAME-1] = '\0';
    g->player_name[1][GAME_MAX_USERNAME-1] = '\0';

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

void game_print(const game_t *g, char *dest, size_t n) {
    if (!g) return;

    /* If dest is provided, write the compact client-friendly
     * single-line representation (reuse game_to_string).
     */
    if (dest != NULL && n > 0) {
        game_to_string(g, dest, n);
        return;
    }

    /* Otherwise, print the ASCII art board to stdout (legacy behavior). */
    printf("Scores: Player A = %d, Player B = %d\n", g->score[0], g->score[1]);
    printf("Turn: Player %s\n", g->turn == PLAYER_A ? "A" : "B");
    printf("\n");
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

    /* check it's the player's turn */
    if (p != g->turn) return false; /* wrong player's turn */

    if (!game_is_move_legal(g, p, pit_index)) return false;

    int seeds = g->pits[pit_index];
    g->pits[pit_index] = 0;
    int index = pit_index;
    while (seeds > 0) {
        index = (index + 1) % N_PITS;
        g->pits[index]++;
        seeds--;
    }

    /* simple capture logic: capture when last seed lands on opponent half with 2 or 3 seeds */
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

    /* record move in history */
    if (g->moves_len < GAME_MAX_MOVES) {
        g->moves[g->moves_len].player = p;
        g->moves[g->moves_len].pit_index = pit_index;
        g->moves_len++;
    }

    /* flip turn */
    g->turn = (g->turn == PLAYER_A) ? PLAYER_B : PLAYER_A;

    /* update state */
    if (g->state == GAME_STATE_NEW) g->state = GAME_STATE_ONGOING;
    if (game_is_over(g)) g->state = GAME_STATE_FINISHED;

    return true;
}


/* Minimal helper implementations */
bool game_is_move_legal(const game_t *g, player_t p, int pit_index) {
    if (!g) return false;
    if (p != g->turn) return false;
    if (p == PLAYER_A) {
        if (pit_index < 0 || pit_index >= N_PITS/2) return false;
    } else {
        if (pit_index < N_PITS/2 || pit_index >= N_PITS) return false;
    }
    if (g->pits[pit_index] == 0) return false;
    return true;
}


/* Serialize a compact single-line representation of the game suitable
 * for sending to clients. The format is:
 *  <id> <turn> <scoreA> <scoreB> <p0> ... <p11> <moves_len> <state> <nameA> <nameB>
 * Player names will have spaces replaced by underscores to keep tokens
 * single-word.
 */
char *game_to_string(const game_t *g, char *dest, size_t n) {
    if (!g || !dest || n == 0) return dest;
    size_t pos = 0;
    int r = snprintf(dest + pos, (pos < n) ? (n - pos) : 0,
                     "%llu %c %d %d",
                     (unsigned long long)g->id,
                     (g->turn == PLAYER_A) ? 'A' : (g->turn == PLAYER_B) ? 'B' : '?',
                     g->score[0], g->score[1]);
    if (r < 0) { dest[0] = '\0'; return dest; }
    pos += (size_t)r;

    for (int i = 0; i < N_PITS; ++i) {
        if (pos >= n) break;
        r = snprintf(dest + pos, n - pos, " %d", g->pits[i]);
        if (r < 0) break;
        pos += (size_t)r;
    }

    if (pos < n) {
        r = snprintf(dest + pos, n - pos, " %d %d", g->moves_len, (int)g->state);
        if (r > 0) pos += (size_t)r;
    }

    /* Append sanitized player names */
    char a[GAME_MAX_USERNAME+1] = {0};
    char b[GAME_MAX_USERNAME+1] = {0};
    if (g->player_name[0][0]) {
        strncpy(a, g->player_name[0], GAME_MAX_USERNAME);
        a[GAME_MAX_USERNAME] = '\0';
        for (size_t i = 0; a[i]; ++i) if (a[i] == ' ') a[i] = '_';
    }
    if (g->player_name[1][0]) {
        strncpy(b, g->player_name[1], GAME_MAX_USERNAME);
        b[GAME_MAX_USERNAME] = '\0';
        for (size_t i = 0; b[i]; ++i) if (b[i] == ' ') b[i] = '_';
    }
    if (pos < n) {
        snprintf(dest + pos, n - pos, " %s %s", a, b);
    }

    dest[n-1] = '\0';
    return dest;
}


