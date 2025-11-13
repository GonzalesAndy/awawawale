#include "persist.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Format :
 * version:1
 * id:<uint64>
 * player_a:<name>
 * player_b:<name>
 * score_a:<int>
 * score_b:<int>
 * moves_len:<n>
 * then n lines: <player_index> <pit_index>
 *
 * Only allowed when game is finished.
 */
int game_save_record(const game_t *g, const char *path)
{
    if (!g || !path)
        return -1;
    if (g->state != GAME_STATE_FINISHED)
        return -1;

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

    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return -1;
    }

    return 0;
}

/*
 * Initialize a game instance from a recorded file.
 * Returns 0 on success, -1 on failure.
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
            player_a[strcspn(player_a, "\r\n")] = '\0';
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

    /* Ensures the game is reconstructed correctly */
    game_t tmp;
    game_init(&tmp, player_a[0] ? player_a : NULL, player_b[0] ? player_b : NULL);
    tmp.id = id;
    tmp.moves_len = 0;

    for (int i = 0; i < moves_len; ++i) {
        game_make_move(&tmp, parsed_moves[i].player, parsed_moves[i].pit_index);
    }

    *g = tmp; /* struct copy */

    g->moves_len = moves_len;
    for (int i = 0; i < moves_len; ++i)
        g->moves[i] = parsed_moves[i];

    g->score[0] = score_a;
    g->score[1] = score_b;
    g->state = GAME_STATE_FINISHED;
    g->id = (uint64_t)id;

    return 0;
}
