#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

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

void game_print(const game_t *g) {
    // prints the game state
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

    if (p == PLAYER_A && index >= N_PITS/2) {
        int j = index;
        while (j >= N_PITS/2 && (g->pits[j] == 2 || g->pits[j] == 3)) {
            g->score[0] += g->pits[j];
            g->pits[j] = 0;
            j--;
        }
    } else if (p == PLAYER_B && index < N_PITS/2) {
        int j = index;
        while (j >= 0 && j < N_PITS/2 && (g->pits[j] == 2 || g->pits[j] == 3)) {
            g->score[1] += g->pits[j];
            g->pits[j] = 0;
            j--;
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

static void append_str(char *dest, size_t n, size_t *used, const char *fmt, ...)
{
    if (*used >= n)
        return;

    va_list args;
    va_start(args, fmt);
    int w = vsnprintf(dest + *used, n - *used, fmt, args);
    va_end(args);

    if (w > 0)
        *used += (size_t)w;
}

char *game_to_string(const game_t *g, char *dest, size_t n)
{
    if (!g || !dest || n == 0)
        return NULL;

    size_t used = 0;
    int half = N_PITS / 2;

    // Header
    append_str(dest, n, &used, "====================================\n");
    append_str(dest, n, &used, " Scores:\n");
    append_str(dest, n, &used, "   Player A (%s): %2d     Player B (%s): %2d\n", g->player_name[0], g->score[0], g->player_name[1], g->score[1]);
    append_str(dest, n, &used, " Turn: Player %c\n", g->turn == PLAYER_A ? 'A' : 'B');
    append_str(dest, n, &used, "====================================\n\n");

    // Top indices (B side)
    append_str(dest, n, &used, "          ");
    for (int i = N_PITS - 1; i >= half; i--)
        append_str(dest, n, &used, " %2d  ", i);
    append_str(dest, n, &used, "\n");

    // Top border
    append_str(dest, n, &used, "        ┌");
    for (int i = 0; i < half - 1; i++)
        append_str(dest, n, &used, "────┬");
    append_str(dest, n, &used, "────┐\n");

    // Player B row
    append_str(dest, n, &used, "Player B│");
    for (int i = N_PITS - 1; i >= half; i--)
        append_str(dest, n, &used, " %2d │", g->pits[i]);
    append_str(dest, n, &used, " ← sens du jeu\n");

    // Separator
    append_str(dest, n, &used, "        ├");
    for (int i = 0; i < half - 1; i++)
        append_str(dest, n, &used, "────┼");
    append_str(dest, n, &used, "────┤\n");

    // Player A row
    append_str(dest, n, &used, "Player A│");
    for (int i = 0; i < half; i++)
        append_str(dest, n, &used, " %2d │", g->pits[i]);
    append_str(dest, n, &used, " → sens du jeu\n");

    // Bottom border
    append_str(dest, n, &used, "        └");
    for (int i = 0; i < half - 1; i++)
        append_str(dest, n, &used, "────┴");
    append_str(dest, n, &used, "────┘\n");

    // Bottom indices
    append_str(dest, n, &used, "          ");
    for (int i = 0; i < half; i++)
        append_str(dest, n, &used, " %2d  ", i);
    append_str(dest, n, &used, "\n");

    // Final termination
    if (used >= n)
        dest[n - 1] = '\0';
    else
        dest[used] = '\0';

    return dest;
}

/*
 * Persist a finished game's minimal record to a simple text format.
 * Format (line-based):
 * version:1
 * id:<uint64>
 * player_a:<name>
 * player_b:<name>
 * score_a:<int>
 * score_b:<int>
 * moves_len:<n>
 * then n lines: <player_index> <pit_index>
 *
 * Only allowed when game state is GAME_STATE_FINISHED (per request).
 */
int game_save_record(const game_t *g, const char *path)
{
    if (!g || !path)
        return -1;
    if (g->state != GAME_STATE_FINISHED)
        return -1; /* only export finished games */

    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (!f)
        return -1;

    fprintf(f, "version:1\n");
    fprintf(f, "id:%llu\n", (unsigned long long)g->id);
    fprintf(f, "player_a:%s\n", g->player_name[0]);
    fprintf(f, "player_b:%s\n", g->player_name[1]);
    fprintf(f, "score_a:%d\n", g->score[0]);
    fprintf(f, "score_b:%d\n", g->score[1]);
    fprintf(f, "moves_len:%d\n", g->moves_len);

    for (int i = 0; i < g->moves_len; ++i) {
        fprintf(f, "%d %d\n", (int)g->moves[i].player, g->moves[i].pit_index);
    }

    if (fclose(f) != 0) {
        remove(tmp_path);
        return -1;
    }

    /* atomic replace */
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return -1;
    }

    return 0;
}

/*
 * Load a saved game record written by game_save_record into g.
 * The function will initialize a temporary game and replay moves to
 * reconstruct the final board, then populate `g` fields and the moves
 * history. Returns 0 on success.
 */
int game_load_record(game_t *g, const char *path)
{
    if (!g || !path)
        return -1;

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char line[512];
    unsigned long long id = 0;
    char player_a[GAME_MAX_USERNAME] = "";
    char player_b[GAME_MAX_USERNAME] = "";
    int score_a = 0, score_b = 0;
    int moves_len = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "version:", 8) == 0) continue; /* ignore for now */
        else if (strncmp(line, "id:", 3) == 0) {
            id = (unsigned long long)strtoull(line + 3, NULL, 10);
        } else if (strncmp(line, "player_a:", 9) == 0) {
            strncpy(player_a, line + 9, GAME_MAX_USERNAME - 1);
            /* strip newline */ player_a[strcspn(player_a, "\r\n")] = '\0';
        } else if (strncmp(line, "player_b:", 9) == 0) {
            strncpy(player_b, line + 9, GAME_MAX_USERNAME - 1);
            player_b[strcspn(player_b, "\r\n")] = '\0';
        } else if (strncmp(line, "score_a:", 8) == 0) {
            score_a = atoi(line + 8);
        } else if (strncmp(line, "score_b:", 8) == 0) {
            score_b = atoi(line + 8);
        } else if (strncmp(line, "moves_len:", 10) == 0) {
            moves_len = atoi(line + 10);
            break; /* next lines are moves */
        }
    }

    if (moves_len < 0 || moves_len > GAME_MAX_MOVES) {
        fclose(f);
        return -1;
    }

    /* read move lines */
    game_move_t parsed_moves[GAME_MAX_MOVES];
    int idx = 0;
    while (idx < moves_len && fgets(line, sizeof(line), f)) {
        int player_i = 0, pit = 0;
        if (sscanf(line, "%d %d", &player_i, &pit) == 2) {
            parsed_moves[idx].player = (player_t)player_i;
            parsed_moves[idx].pit_index = pit;
            idx++;
        }
    }

    fclose(f);

    if (idx != moves_len) return -1;

    /* Reconstruct final board by replaying moves on a fresh game */
    game_t tmp;
    game_init(&tmp, player_a[0] ? player_a : NULL, player_b[0] ? player_b : NULL);
    tmp.id = id;
    tmp.moves_len = 0; /* game_make_move will populate tmp.moves; we don't care */

    for (int i = 0; i < moves_len; ++i) {
        game_make_move(&tmp, parsed_moves[i].player, parsed_moves[i].pit_index);
    }

    /* copy reconstructed state into g */
    *g = tmp; /* struct copy */

    /* overwrite moves array with the parsed moves so history matches file */
    g->moves_len = moves_len;
    for (int i = 0; i < moves_len; ++i)
        g->moves[i] = parsed_moves[i];

    g->score[0] = score_a;
    g->score[1] = score_b;
    g->state = GAME_STATE_FINISHED;
    g->id = (uint64_t)id;

    return 0;
}
