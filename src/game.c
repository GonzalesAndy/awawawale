#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

//Remind pour plus tard : penser à mettre à jour la strucutre de la game de manière meilleure...


void game_init(game_t *g, const char *player_a_name, const char *player_b_name) {
    for (int i = 0; i < N_PITS; ++i) g->pits[i] = SEEDS_PER_PIT;
    g->score[0] = g->score[1] = 0;
    /* seed RNG once for random start */
    srand((unsigned)time(NULL));
    g->turn = (rand() & 1) ? PLAYER_A : PLAYER_B; // random start
    g->id = 0;
    /* copy player names if provided */
    if (player_a_name) {
        strncpy(g->player_name[0], player_a_name, GAME_MAX_USERNAME-1);
        g->player_name[0][GAME_MAX_USERNAME-1] = '\0';
    } else g->player_name[0][0] = '\0';
    if (player_b_name) {
        strncpy(g->player_name[1], player_b_name, GAME_MAX_USERNAME-1);
        g->player_name[1][GAME_MAX_USERNAME-1] = '\0';
    } else g->player_name[1][0] = '\0';
    g->state = GAME_STATE_NEW;
    g->moves_len = 0;
    g->private_mode = false;
    g->allowed_spectators_count = 0;
}

static bool pit_belongs_to(player_t p, int pit) {
    if (pit < 0 || pit >= N_PITS) return false;
    if (p == PLAYER_A) return (pit >= 0 && pit < N_PITS/2);
    return (pit >= N_PITS/2 && pit < N_PITS);
}



bool game_is_over(const game_t *g) {
    if (!g) return true;
    /* majority captured */
    int total = N_PITS * SEEDS_PER_PIT;
    if (g->score[0] > total/2 || g->score[1] > total/2) return true;
    /* one side has no seeds but does not take in charge famine rule */
    int sumA = 0, sumB = 0;
    for (int i = 0; i < N_PITS/2; ++i) sumA += g->pits[i];
    for (int i = N_PITS/2; i < N_PITS; ++i) sumB += g->pits[i];
    if (sumA == 0 || sumB == 0) return true;
    return false;
}

void game_print(const game_t *g) {
    // prints the game state
    printf("Scores: Player A = %d, Player B = %d\n", g->score[0], g->score[1]);
    if (g->player_name[0][0] || g->player_name[1][0]) {
        printf("Turn: Player %s (%s)\n", g->turn == PLAYER_A ? "A" : "B",
               g->turn == PLAYER_A ? g->player_name[0] : g->player_name[1]);
    } else {
        printf("Turn: Player %s\n", g->turn == PLAYER_A ? "A" : "B");
    }
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
        seeds--;
    }

    /* capture: collect from opponent side going backwards while pits have 2 or 3 */
    player_t opponent = (p == PLAYER_A) ? PLAYER_B : PLAYER_A;
    int captured = 0;
    int cur = index;
    while (pit_belongs_to(opponent, cur) && (g->pits[cur] == 2 || g->pits[cur] == 3)) {
        captured += g->pits[cur];
        g->pits[cur] = 0;
        cur = (cur - 1 + N_PITS) % N_PITS;
    }
    if (captured) g->score[p] += captured;

    /* record move in history */
    if (g->moves_len < GAME_MAX_MOVES) {
        g->moves[g->moves_len].player = p;
        g->moves[g->moves_len].pit_index = pit_index;
        g->moves_len++;
    }

    /* advance turn and update state */
    g->turn = opponent;
    if (g->state == GAME_STATE_NEW) g->state = GAME_STATE_ONGOING;
    if (game_is_over(g)) g->state = GAME_STATE_FINISHED;
    return true;
    
}

// saves the game state to a file
int game_save_record(const char* state, const char *path) {
    if (!state || !path) return -1;
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%s", state);
    fclose(f);
    return 0;
}


// Obtain a text representation of the board into the provided buffer.
// The resulting string is NUL terminated. Returns dest on success.
char *game_to_string(const game_t *g, char *dest, size_t n){
    if (!g || !dest || n == 0) return NULL;
    size_t used = 0;
    int rv = 0;
    /* header + version */
    rv = snprintf(dest + used, n > used ? n - used : 0, "v1|p|");
    if (rv < 0 || (size_t)rv >= (n > used ? n - used : 0)) return NULL;
    used += (size_t)rv;

    /* pits */
    for (int i = 0; i < N_PITS; ++i) {
        if (i == 0)
            rv = snprintf(dest + used, n - used, "%d", g->pits[i]);
        else
            rv = snprintf(dest + used, n - used, ",%d", g->pits[i]);
        if (rv < 0 || (size_t)rv >= n - used) return NULL;
        used += (size_t)rv;
    }

    /* scores */
    rv = snprintf(dest + used, n - used, "|s|%d,%d|t|", g->score[0], g->score[1]);
    if (rv < 0 || (size_t)rv >= n - used) return NULL;
    used += (size_t)rv;

    /* turn */
    char t = (g->turn == PLAYER_A) ? 'A' : (g->turn == PLAYER_B ? 'B' : 'T');
    rv = snprintf(dest + used, n - used, "%c\n", t);
    if (rv < 0 || (size_t)rv >= n - used) return NULL;
    used += (size_t)rv;

    return dest;
}

int game_load_record(game_t *g, const char *path) {
    if (!g || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char buf[512];
    while (fgets(buf, sizeof buf, f)) {
        
    }
    fclose(f);
    return 0;
}
