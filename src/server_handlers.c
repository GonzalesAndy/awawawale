#include "server_handlers.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <stdlib.h>

static void safe_send(int fd, const char *s)
{
    if (fd < 0 || !s) return;
    size_t len = strlen(s), sent = 0;
    ssize_t n;
    while (sent < len && (n = send(fd, s + sent, len - sent, 0)) > 0)
        sent += n;
}

static const char *parse_cmd(char *line, char **args)
{
    size_t L = strlen(line);
    if (L && (line[L - 1] == '\n' || line[L - 1] == '\r'))
        line[--L] = '\0';
    char *sp = strchr(line, ' ');
    if (sp)
    {
        *sp = '\0';
        *args = sp + 1;
    }
    else
    {
        *args = NULL;
    }
    return line;
}

static int is_friend_of(server_state_t *state, const char *owner, const char *candidate)
{
    if (!state || !owner || !candidate) return 0;
    char friends[CLIENT_MAX_FRIENDS][GAME_MAX_USERNAME];
    int n = server_friend_list(state, owner, friends, CLIENT_MAX_FRIENDS);
    for (int i = 0; i < n; ++i)
        if (strcmp(friends[i], candidate) == 0) return 1;
    return 0;
}

static int handle_register(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char uname[GAME_MAX_USERNAME];
    strncpy(uname, args, sizeof(uname) - 1);
    uname[sizeof(uname) - 1] = '\0';
    if (self->name[0] != '\0' && strcmp(self->name, uname) != 0)
        server_mark_user_offline(state, self->name);
    if (server_register_user(state, uname, self->fd) == 0)
    {
        strncpy(self->name, uname, sizeof(self->name) - 1);
        self->name[sizeof(self->name) - 1] = '\0';
        char resp[PROTO_MAX_LINE];
        snprintf(resp, sizeof(resp), "REGISTERED %s\n", self->name);
        safe_send(self->fd, resp);
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR username %s already in use\n", uname);
    safe_send(self->fd, resp);
    return -1;
}

static int handle_bio(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self;
    (void)clients;
    if (!args)
        return -1;
    char *username = strtok(args, " ");
    char *bio_text = strtok(NULL, "");
    if (username && bio_text)
    {
        if (server_set_user_bio(state, username, bio_text) == 0)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "BIO_SET %s\n", username);
            safe_send(self->fd, resp);
            return 0;
        }
        char resp[PROTO_MAX_LINE];
        snprintf(resp, sizeof(resp), "ERROR user %s not found\n", username);
        safe_send(self->fd, resp);
    }
    return -1;
}

static int handle_bio_show(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *requester = strtok(args, " ");
    char *target = requester ? strtok(NULL, " \n") : NULL;
    if (!target)
        return -1;
    char biobuf[CLIENT_MAX_BIO];
    if (server_show_user_bio(state, requester, target, biobuf, sizeof(biobuf)) == 0)
    {
        // Send as a block to preserve newlines
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "BIO_BEGIN %s\n%s\nBIO_END\n", target, biobuf);
        // Note: snprintf with %s and biobuf will include embedded newlines
        // but ensure resp buffer size is respected
        safe_send(self->fd, resp);
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR user %s not found\n", target);
    safe_send(self->fd, resp);
    return -1;
}

static int handle_list_users(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)args;
    (void)clients;
    char out[PROTO_MAX_LINE];
    out[0] = '\0';
    strncat(out, CMD_USERS, sizeof(out) - strlen(out) - 1);
    char users[SERVER_MAX_USERS][GAME_MAX_USERNAME];
    int n = server_list_online_users(state, users, SERVER_MAX_USERS);
    for (int j = 0; j < n; ++j)
    {
        strncat(out, " ", sizeof(out) - strlen(out) - 1);
        strncat(out, users[j], sizeof(out) - strlen(out) - 1);
    }
    strncat(out, "\n", sizeof(out) - strlen(out) - 1);
    safe_send(self->fd, out);
    return 0;
}

// TODO : Maybe do not permit sending a challenge if u already have a running game with him
static int handle_challenge(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self;
    if (!args)
        return -1;
    char *from = strtok(args, " ");
    char *to = strtok(NULL, " ");
    if (!from || !to)
        return -1;
    if (strcmp(from, to) == 0)
    {
        safe_send(self->fd, "ERROR cannot challenge yourself\n");
        return -1;
    }
    int rv = server_create_challenge(state, from, to);
    if (rv == 0)
    {
        int notified = 0;
        for (int j = 0; j < SERVER_NET_MAX_CLIENTS; ++j)
        {
            if (clients[j].fd != -1 && strcmp(clients[j].name, to) == 0)
            {
                char out[PROTO_MAX_LINE];
                snprintf(out, sizeof(out), "CHALLENGE RECEIVED FROM %s\n", from);
                safe_send(clients[j].fd, out);
                notified = 1;
                break;
            }
        }
        safe_send(self->fd, "CHALLENGE_SENT\n");
        if (!notified)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "WARNING target %s not online\n", to);
            safe_send(self->fd, resp);
        }
        return 0;
    }
    safe_send(self->fd, "ERROR no challenge slots available\n");
    return -1;
}

static int handle_accept(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self;
    if (!args)
        return -1;
    char *acceptor = strtok(args, " ");
    char *challenger = strtok(NULL, " ");
    if (!acceptor || !challenger)
        return -1;
    uint64_t gid = 0;
    int rv = server_accept_challenge(state, challenger, acceptor, &gid);
    if (rv == 0)
    {
        uint64_t created_gid = 0;
        if (server_create_game_from_challenge(state, challenger, acceptor, &created_gid) == 0)
        {
            gid = created_gid;
        }
        int notified = 0;
        for (int j = 0; j < SERVER_NET_MAX_CLIENTS; ++j)
        {
            if (clients[j].fd != -1 && strcmp(clients[j].name, challenger) == 0)
            {
                char out[PROTO_MAX_LINE];
                snprintf(out, sizeof(out), "ACCEPTED %s %s %lu\n", acceptor, challenger, gid);
                safe_send(clients[j].fd, out);
                snprintf(out, sizeof(out), "FOCUSED %lu\n", (unsigned long)gid);
                safe_send(clients[j].fd, out);
                clients[j].focused_game_id = gid;
                notified = 1;
                break;
            }
        }
        char out2[PROTO_MAX_LINE];
        snprintf(out2, sizeof(out2), "ACCEPT_SENT %s %s %lu\n", acceptor, challenger, gid);
        safe_send(self->fd, out2);
        snprintf(out2, sizeof(out2), "FOCUSED %lu\n", (unsigned long)gid);
        safe_send(self->fd, out2);
        self->focused_game_id = gid;
        if (!notified)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "WARNING challenger %s not online\n", challenger);
            safe_send(self->fd, resp);
        }
        // Also show initial board to both players
        game_t *g = NULL;
        if (server_get_game(state, gid, &g) == 0 && g)
        {
            char board[PROTO_MAX_LINE];
            game_to_string(g, board, sizeof(board));
            char init_msg[PROTO_MAX_LINE];
            snprintf(init_msg, sizeof(init_msg), "\n%s\n", board);
            for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
            {
                if (clients[i].fd == -1)
                    continue;
                if (strcmp(clients[i].name, acceptor) == 0 || strcmp(clients[i].name, challenger) == 0)
                {
                    safe_send(clients[i].fd, init_msg);
                }
            }
        }
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR No active challenge from %s to %s\n", challenger, acceptor);
    safe_send(self->fd, resp);
    return -1;
}

static int handle_move(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *from = strtok(args, " ");
    char *gid_s = strtok(NULL, " ");
    char *pit_s = strtok(NULL, " \n");
    if (!from || !pit_s)
        return -1;
    uint64_t gid = 0;
    if (gid_s)
        gid = strtoull(gid_s, NULL, 10);
    if (gid == 0)
        gid = self->focused_game_id;
    if (gid == 0)
    {
        safe_send(self->fd, "ERROR no focused game\n");
        return -1;
    }

    game_t *g = NULL;
    if (server_get_game(state, gid, &g) != 0 || !g)
    {
        safe_send(self->fd, "ERROR game not found\n");
        return -1;
    }

    player_t p = (strcmp(from, g->player_name[0]) == 0) ? PLAYER_A : (strcmp(from, g->player_name[1]) == 0 ? PLAYER_B : (player_t)2);
    if (p == (player_t)2)
    {
        safe_send(self->fd, "ERROR not a player in this game\n");
        return -1;
    }
    int pit = atoi(pit_s);
    if (!game_is_move_legal(g, p, pit))
    {
        safe_send(self->fd, "ERROR illegal move\n");
        return -1;
    }
    if (!game_make_move(g, p, pit))
    {
        safe_send(self->fd, "ERROR move failed\n");
        return -1;
    }

    char board[PROTO_MAX_LINE];
    game_to_string(g, board, sizeof(board));

    char msg[PROTO_MAX_LINE];
    snprintf(msg, sizeof(msg), "\n%s\n", board);
    for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
    {
        if (clients[i].fd == -1)
            continue;
        if (strcmp(clients[i].name, g->player_name[0]) == 0 || strcmp(clients[i].name, g->player_name[1]) == 0)
        {
            safe_send(clients[i].fd, msg);
        }
        else if (clients[i].observed_game_id == g->id)
        {
            char obs_msg[PROTO_MAX_LINE + 64];
            snprintf(obs_msg, sizeof(obs_msg), "[SPECTATING g-%lu]\n%s", (unsigned long)g->id, msg);
            safe_send(clients[i].fd, obs_msg);
        }
    }
    if (g->state == GAME_STATE_FINISHED)
    {
        char out[PROTO_MAX_LINE];
        if (g->score[0] > g->score[1])
        {
            snprintf(out, sizeof(out), "GAME_OVER %s %d %s %d WINNER %s\n", g->player_name[0], g->score[0], g->player_name[1], g->score[1], g->player_name[0]);
        }
        else if (g->score[1] > g->score[0])
        {
            snprintf(out, sizeof(out), "GAME_OVER %s %d %s %d WINNER %s\n", g->player_name[0], g->score[0], g->player_name[1], g->score[1], g->player_name[1]);
        }
        else
        {
            snprintf(out, sizeof(out), "GAME_OVER %s %d %s %d DRAW\n", g->player_name[0], g->score[0], g->player_name[1], g->score[1]);
        }
        for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
        {
            if (clients[i].fd == -1)
                continue;
            if (strcmp(clients[i].name, g->player_name[0]) == 0 || strcmp(clients[i].name, g->player_name[1]) == 0)
            {
                safe_send(clients[i].fd, out);
            }
        }

        // Persist finished game to replays/g-<id>.rec
        char replay_dir[256] = "replays";
        char replay_path[512];
        snprintf(replay_path, sizeof(replay_path), "%s/g-%lu.rec", replay_dir, (unsigned long)g->id);
        {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", replay_dir);
            system(cmd);
        }
        game_save_record(g, replay_path);
        server_remove_game(state, g->id);
    }
    return 0;
}

static int handle_get_replay(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)state; (void)clients;
    if (!self || !args) return -1;
    char *requester = strtok(args, " ");
    char *gid_s = strtok(NULL, " \n");
    if (!requester || !gid_s) return -1;
    uint64_t gid = strtoull(gid_s, NULL, 10);
    if (gid == 0) return -1;
    
    char replay_path[512];
    snprintf(replay_path, sizeof(replay_path), "replays/g-%lu.rec", (unsigned long)gid);
    
    // Load the game record
    game_t loaded_game;
    if (game_load_record(&loaded_game, replay_path) != 0) {
        safe_send(self->fd, "ERROR replay not found\n");
        return -1;
    }
    
    // Enter replay mode
    self->in_replay_mode = true;
    self->replay_gid = gid;
    self->replay_current_move = -1; // Start at initial state
    
    // Send initial message
    safe_send(self->fd, "\033[2J\033[H"); // Clear screen and move cursor to top
    char header[PROTO_MAX_LINE];
    snprintf(header, sizeof(header), "=== REPLAY MODE: Game %lu ===\n", (unsigned long)gid);
    safe_send(self->fd, header);
    snprintf(header, sizeof(header), "Players: %s vs %s\n", loaded_game.player_name[0], loaded_game.player_name[1]);
    safe_send(self->fd, header);
    snprintf(header, sizeof(header), "Final Score: %d - %d\n", loaded_game.score[0], loaded_game.score[1]);
    safe_send(self->fd, header);
    snprintf(header, sizeof(header), "Total moves: %d\n\n", loaded_game.moves_len);
    safe_send(self->fd, header);
    
    // Show initial board state
    game_t initial_game;
    game_init(&initial_game, loaded_game.player_name[0], loaded_game.player_name[1]);
    initial_game.id = loaded_game.id;
    
    char board[PROTO_MAX_LINE];
    game_to_string(&initial_game, board, sizeof(board));
    char msg[PROTO_MAX_LINE];
    snprintf(msg, sizeof(msg), "Move 0/%d (Initial State):\n%s\n", loaded_game.moves_len, board);
    safe_send(self->fd, msg);
    
    safe_send(self->fd, "\nCommands: NEXT | PREVIOUS | QUIT\n> ");
    
    return 0;
}

static int handle_replay_next(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)state; (void)clients; (void)args;
    if (!self || !self->in_replay_mode) return -1;
    
    char replay_path[512];
    snprintf(replay_path, sizeof(replay_path), "replays/g-%lu.rec", (unsigned long)self->replay_gid);
    
    game_t loaded_game;
    if (game_load_record(&loaded_game, replay_path) != 0) {
        safe_send(self->fd, "ERROR loading replay\n");
        self->in_replay_mode = false;
        return -1;
    }
    
    // Check if we can advance
    if (self->replay_current_move >= loaded_game.moves_len - 1) {
        safe_send(self->fd, "Already at the last move\n> ");
        return 0;
    }
    
    // Advance to next move
    self->replay_current_move++;
    
    // Reconstruct game state up to current move
    game_t replay_game;
    game_init(&replay_game, loaded_game.player_name[0], loaded_game.player_name[1]);
    replay_game.id = loaded_game.id;
    
    for (int i = 0; i <= self->replay_current_move; ++i) {
        game_make_move(&replay_game, loaded_game.moves[i].player, loaded_game.moves[i].pit_index);
    }
    
    // Clear screen and display
    safe_send(self->fd, "\033[2J\033[H"); // Clear screen
    char header[PROTO_MAX_LINE];
    snprintf(header, sizeof(header), "=== REPLAY MODE: Game %lu ===\n", (unsigned long)self->replay_gid);
    safe_send(self->fd, header);
    snprintf(header, sizeof(header), "Players: %s vs %s\n", loaded_game.player_name[0], loaded_game.player_name[1]);
    safe_send(self->fd, header);
    
    char board[PROTO_MAX_LINE];
    game_to_string(&replay_game, board, sizeof(board));
    char msg[PROTO_MAX_LINE];
    const char *player_name = loaded_game.moves[self->replay_current_move].player == PLAYER_A ? 
                              loaded_game.player_name[0] : loaded_game.player_name[1];
    snprintf(msg, sizeof(msg), "Move %d/%d: %s played pit %d\n%s\n", 
             self->replay_current_move + 1, loaded_game.moves_len,
             player_name, loaded_game.moves[self->replay_current_move].pit_index,
             board);
    safe_send(self->fd, msg);
    
    safe_send(self->fd, "\nCommands: NEXT | PREVIOUS | QUIT\n> ");
    
    return 0;
}

static int handle_replay_prev(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)state; (void)clients; (void)args;
    if (!self || !self->in_replay_mode) return -1;
    
    char replay_path[512];
    snprintf(replay_path, sizeof(replay_path), "replays/g-%lu.rec", (unsigned long)self->replay_gid);
    
    game_t loaded_game;
    if (game_load_record(&loaded_game, replay_path) != 0) {
        safe_send(self->fd, "ERROR loading replay\n");
        self->in_replay_mode = false;
        return -1;
    }
    
    // Check if we can go back
    if (self->replay_current_move < 0) {
        safe_send(self->fd, "Already at the initial state\n> ");
        return 0;
    }
    
    // Go back one move
    self->replay_current_move--;
    
    // Clear screen
    safe_send(self->fd, "\033[2J\033[H");
    char header[PROTO_MAX_LINE];
    snprintf(header, sizeof(header), "=== REPLAY MODE: Game %lu ===\n", (unsigned long)self->replay_gid);
    safe_send(self->fd, header);
    snprintf(header, sizeof(header), "Players: %s vs %s\n", loaded_game.player_name[0], loaded_game.player_name[1]);
    safe_send(self->fd, header);
    
    // Reconstruct game state
    game_t replay_game;
    game_init(&replay_game, loaded_game.player_name[0], loaded_game.player_name[1]);
    replay_game.id = loaded_game.id;
    
    if (self->replay_current_move >= 0) {
        for (int i = 0; i <= self->replay_current_move; ++i) {
            game_make_move(&replay_game, loaded_game.moves[i].player, loaded_game.moves[i].pit_index);
        }
        
        char board[PROTO_MAX_LINE];
        game_to_string(&replay_game, board, sizeof(board));
        char msg[PROTO_MAX_LINE];
        const char *player_name = loaded_game.moves[self->replay_current_move].player == PLAYER_A ? 
                                  loaded_game.player_name[0] : loaded_game.player_name[1];
        snprintf(msg, sizeof(msg), "Move %d/%d: %s played pit %d\n%s\n", 
                 self->replay_current_move + 1, loaded_game.moves_len,
                 player_name, loaded_game.moves[self->replay_current_move].pit_index,
                 board);
        safe_send(self->fd, msg);
    } else {
        // Show initial state
        char board[PROTO_MAX_LINE];
        game_to_string(&replay_game, board, sizeof(board));
        char msg[PROTO_MAX_LINE];
        snprintf(msg, sizeof(msg), "Move 0/%d (Initial State):\n%s\n", loaded_game.moves_len, board);
        safe_send(self->fd, msg);
    }
    
    safe_send(self->fd, "\nCommands: NEXT | PREVIOUS | QUIT\n> ");
    
    return 0;
}

static int handle_replay_exit(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)state; (void)clients; (void)args;
    if (!self || !self->in_replay_mode) return -1;
    
    self->in_replay_mode = false;
    self->replay_gid = 0;
    self->replay_current_move = -1;
    
    safe_send(self->fd, "\033[2J\033[H"); // Clear screen
    safe_send(self->fd, "Exited replay mode\n");
    
    return 0;
}

static int handle_show_games(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    (void)state;
    if (!args)
        return -1;
    char *from = strtok(args, " \n");
    if (!from)
        return -1;

    char out[PROTO_MAX_LINE];
    out[0] = '\0';
    strncat(out, "GAMES", sizeof(out) - strlen(out) - 1);
    for (int i = 0; i < state->games_count; ++i)
    {
        game_t *g = &state->games[i];
        if (strcmp(g->player_name[0], from) == 0 || strcmp(g->player_name[1], from) == 0)
        {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), " %lu", (unsigned long)g->id);
            strncat(out, tmp, sizeof(out) - strlen(out) - 1);
        }
    }
    strncat(out, "\n", sizeof(out) - strlen(out) - 1);
    safe_send(self->fd, out);
    return 0;
}

static int handle_focus(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    (void)state;
    if (!args)
        return -1;
    char *from = strtok(args, " ");
    char *gid_s = strtok(NULL, " \n");
    if (!from || !gid_s)
        return -1;
    uint64_t gid = strtoull(gid_s, NULL, 10);
    int game_found = 0;
    for (int i = 0; i < state->games_count; ++i)
    {
        game_t *g = &state->games[i];
        if (g->id == gid)
        {
            if (strcmp(g->player_name[0], from) != 0 && strcmp(g->player_name[1], from) != 0)
            {
                safe_send(self->fd, "ERROR not a player in this game\n");
                return -1;
            }
            game_found = 1;
            break;
        }
    }
    char out[PROTO_MAX_LINE];
    if (!game_found)
    {
        safe_send(self->fd, "ERROR game not found\n");
        return -1;
    }
    else
    {
        self->focused_game_id = gid;
        snprintf(out, sizeof(out), "FOCUSED %lu\n", (unsigned long)gid);
    }
    safe_send(self->fd, out);
    return 0;
}

static int handle_show_board(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *from = strtok(args, " ");
    char *gid_s = strtok(NULL, " \n");
    (void)from;
    uint64_t gid = gid_s ? strtoull(gid_s, NULL, 10) : self->focused_game_id;
    if (gid == 0)
    {
        safe_send(self->fd, "ERROR no focused game\n");
        return -1;
    }
    game_t *g = NULL;
    if (server_get_game(state, gid, &g) != 0)
    {
        safe_send(self->fd, "ERROR game not found\n");
        return -1;
    }
    char board[PROTO_MAX_LINE];
    game_to_string(g, board, sizeof(board));
    char out[PROTO_MAX_LINE];
    snprintf(out, sizeof(out), "\n%s\n", board);
    safe_send(self->fd, out);
    return 0;
}

static int handle_observe(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!state || !args || !self) return -1;
    char *from = strtok(args, " ");
    char *gid_s = strtok(NULL, " \n");
    if (!from) return -1;
    uint64_t gid = 0;
    if (gid_s) gid = strtoull(gid_s, NULL, 10);
    if (gid == 0) gid = self->focused_game_id;
    if (gid == 0) { safe_send(self->fd, "ERROR no focused game\n"); return -1; }
    game_t *g = NULL;
    if (server_get_game(state, gid, &g) != 0 || !g) { safe_send(self->fd, "ERROR game not found\n"); return -1; }
    // privacy: allow if not private OR friend of either player
    int allowed = 0;
    if (!g->private_mode) allowed = 1;
    /* Do NOT allow a player to spectate their own game */
    if (strcmp(g->player_name[0], from) == 0 || strcmp(g->player_name[1], from) == 0) {
        safe_send(self->fd, "ERROR cannot spectate your own game\n");
        return -1;
    }
    if (is_friend_of(state, g->player_name[0], from) || is_friend_of(state, g->player_name[1], from)) allowed = 1;
    if (!allowed) { safe_send(self->fd, "ERROR not allowed to spectate this private game\n"); return -1; }

    // only one observed game at a time
    self->observed_game_id = gid;

    // send initial board with spectating header
    char board[PROTO_MAX_LINE];
    game_to_string(g, board, sizeof(board));
    char msg[PROTO_MAX_LINE + 64];
    snprintf(msg, sizeof(msg), "[SPECTATING g-%lu]\n\n%s\n", (unsigned long)g->id, board);
    safe_send(self->fd, msg);
    return 0;
}

static int handle_stop_observe(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)state; (void)clients; (void)args;
    if (!self) return -1;
    if (self->observed_game_id == 0) { safe_send(self->fd, "ERROR not currently spectating\n"); return -1; }
    char out[PROTO_MAX_LINE];
    snprintf(out, sizeof(out), "STOPPED_SPECTATING %lu\n", (unsigned long)self->observed_game_id);
    self->observed_game_id = 0;
    safe_send(self->fd, out);
    return 0;
}

static int handle_refuse(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self;
    if (!args)
        return -1;
    char *refuser = strtok(args, " ");
    char *challenger = strtok(NULL, " ");
    if (!refuser || !challenger)
        return -1;
    int rv = server_refuse_challenge(state, challenger, refuser);
    if (rv == 0)
    {
        int notified = 0;
        for (int j = 0; j < SERVER_NET_MAX_CLIENTS; ++j)
        {
            if (clients[j].fd != -1 && strcmp(clients[j].name, challenger) == 0)
            {
                char out[PROTO_MAX_LINE];
                snprintf(out, sizeof(out), "REFUSED %s %s\n", refuser, challenger);
                safe_send(clients[j].fd, out);
                notified = 1;
                break;
            }
        }
        char out2[PROTO_MAX_LINE];
        snprintf(out2, sizeof(out2), "REFUSE_SENT %s %s\n", refuser, challenger);
        safe_send(self->fd, out2);
        if (!notified)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "WARNING challenger %s not online\n", challenger);
            safe_send(self->fd, resp);
        }
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR No active challenge from %s to %s\n", challenger, refuser);
    safe_send(self->fd, resp);
    return -1;
}

static int find_client_by_name(client_t *clients, const char *name)
{
    if (!clients || !name)
        return -1;
    for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
        if (clients[i].fd != -1 && strcmp(clients[i].name, name) == 0)
            return i;
    return -1;
}

static int handle_chat(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)state;
    if (!args)
        return -1;
    char *from = strtok(args, " ");
    char *target = strtok(NULL, " ");
    char *msg = strtok(NULL, "");
    if (!from || !msg)
        return -1;
    if (!target || target[0] == '\0')
    {
        char out[PROTO_MAX_LINE];
        snprintf(out, sizeof(out), "CHAT %s %s\n", from, msg);
        for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
            if (clients[i].fd != -1)
                safe_send(clients[i].fd, out);
        return 0;
    }
    int gi = -1;
    for (int i = 0; i < SERVER_MAX_GROUPS; ++i)
        if (state->groups[i].active && strcmp(state->groups[i].name, target) == 0)
        {
            gi = i;
            break;
        }
    char out[PROTO_MAX_LINE];
    if (gi >= 0)
    {
        if (!server_group_is_member(state, state->groups[gi].name, from))
        {
            safe_send(self->fd, "ERROR not a member of group\n");
            return -1;
        }
        for (int m = 0; m < state->groups[gi].member_count; ++m)
        {
            int idx = find_client_by_name(clients, state->groups[gi].members[m]);
            if (idx >= 0)
            {
                snprintf(out, sizeof(out), "CHAT %s@%s %s\n", from, target, msg);
                safe_send(clients[idx].fd, out);
            }
        }
        return 0;
    }
    // Private chat
    int idx = find_client_by_name(clients, target);
    if (idx < 0)
    {
        safe_send(self->fd, "ERROR target not online\n");
        return -1;
    }
    snprintf(out, sizeof(out), "CHAT %s->%s %s\n", from, target, msg);
    safe_send(clients[idx].fd, out);
    return 0;
}

static int handle_group_create(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *owner = strtok(args, " ");
    char *gname = strtok(NULL, " \n");
    if (!owner || !gname)
        return -1;
    if (server_group_create(state, owner, gname) == 0)
    {
        char out[PROTO_MAX_LINE];
        snprintf(out, sizeof(out), "GROUP_CREATED %s\n", gname);
        safe_send(self->fd, out);
        return 0;
    }
    safe_send(self->fd, "ERROR cannot create group\n");
    return -1;
}

static int handle_group_invite(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *owner = strtok(args, " ");
    char *gname = strtok(NULL, " ");
    char *user = strtok(NULL, " \n");
    if (!owner || !gname || !user)
        return -1;
    if (server_group_invite(state, owner, gname, user) == 0)
    {
        char out[PROTO_MAX_LINE];
        snprintf(out, sizeof(out), "GROUP_INVITED %s %s\n", gname, user);
        safe_send(self->fd, out);
        return 0;
    }
    safe_send(self->fd, "ERROR cannot invite\n");
    return -1;
}

static int handle_group_quit(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *gname = strtok(args, " ");
    char *user = strtok(NULL, " \n");
    if (!gname || !user)
        return -1;
    if (server_group_quit(state, gname, user) == 0)
    {
        char out[PROTO_MAX_LINE];
        snprintf(out, sizeof(out), "GROUP_QUIT %s %s\n", gname, user);
        safe_send(self->fd, out);
        return 0;
    }
    safe_send(self->fd, "ERROR cannot quit group\n");
    return -1;
}

static int handle_friend_add(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *owner = strtok(args, " ");
    char *fuser = strtok(NULL, " \n");
    if (!owner || !fuser)
        return -1;
    if (server_friend_add(state, owner, fuser) == 0)
    {
        char out[PROTO_MAX_LINE];
        snprintf(out, sizeof(out), "FRIEND_ADDED %s\n", fuser);
        safe_send(self->fd, out);
        return 0;
    }
    safe_send(self->fd, "ERROR cannot add friend\n");
    return -1;
}

static int handle_friend_remove(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *owner = strtok(args, " ");
    char *fuser = strtok(NULL, " \n");
    if (!owner || !fuser)
        return -1;
    if (server_friend_remove(state, owner, fuser) == 0)
    {
        char out[PROTO_MAX_LINE];
        snprintf(out, sizeof(out), "FRIEND_REMOVED %s\n", fuser);
        safe_send(self->fd, out);
        return 0;
    }
    safe_send(self->fd, "ERROR cannot remove friend\n");
    return -1;
}

static int handle_list_friends(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args)
        return -1;
    char *owner = strtok(args, " \n");
    if (!owner)
        return -1;
    char out[PROTO_MAX_LINE];
    out[0] = '\0';
    strncat(out, CMD_FRIENDS, sizeof(out) - strlen(out) - 1);
    char friends[CLIENT_MAX_FRIENDS][GAME_MAX_USERNAME];
    int n = server_friend_list(state, owner, friends, CLIENT_MAX_FRIENDS);
    for (int i = 0; i < n; ++i)
    {
        strncat(out, " ", sizeof(out) - strlen(out) - 1);
        strncat(out, friends[i], sizeof(out) - strlen(out) - 1);
    }
    strncat(out, "\n", sizeof(out) - strlen(out) - 1);
    safe_send(self->fd, out);
    return 0;
}

static int handle_list_games(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!state || !self) return -1;
    char *requester = NULL;
    if (self->name[0] != '\0')
        requester = self->name;
    if (!requester) return -1;

    char out[PROTO_MAX_LINE];
    out[0] = '\0';
    strncat(out, "ONGOING_GAMES", sizeof(out) - strlen(out) - 1);
    for (int i = 0; i < state->games_count; ++i)
    {
        game_t *g = &state->games[i];
        int visible = 1;
        if (g->private_mode)
        {
            // visible if requester is one of the players
            if (strcmp(g->player_name[0], requester) == 0 || strcmp(g->player_name[1], requester) == 0)
                visible = 1;
            else if (is_friend_of(state, g->player_name[0], requester) || is_friend_of(state, g->player_name[1], requester))
                visible = 1;
            else
                visible = 0;
        }
        if (visible)
        {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), " %lu:%s:%s", (unsigned long)g->id, g->player_name[0], g->player_name[1]);
            strncat(out, tmp, sizeof(out) - strlen(out) - 1);
        }
    }
    strncat(out, "\n", sizeof(out) - strlen(out) - 1);
    safe_send(self->fd, out);
    return 0;
}

static int handle_set_private(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!state || !args) return -1;
    char *from = strtok(args, " ");
    char *gid_s = strtok(NULL, " ");
    char *flag_s = strtok(NULL, " \n");
    if (!from || !flag_s) return -1;
    uint64_t gid;
    if (gid_s)
        gid  = strtoull(gid_s, NULL, 10);
    if (gid == 0)
        gid = self->focused_game_id;
    if (gid == 0)
    {
        safe_send(self->fd, "ERROR no focused game\n");
        return -1;
    }
    int flag = atoi(flag_s);
    game_t *g = NULL;
    if (server_get_game(state, gid, &g) != 0 || !g)
    {
        safe_send(self->fd, "ERROR game not found\n");
        return -1;
    }
    // Only players in the game may toggle privacy
    if (strcmp(g->player_name[0], from) != 0 && strcmp(g->player_name[1], from) != 0)
    {
        safe_send(self->fd, "ERROR not a player in this game\n");
        return -1;
    }
    g->private_mode = flag ? true : false;
    char out[PROTO_MAX_LINE];
    snprintf(out, sizeof(out), "PRIVATE_SET %lu %d\n", (unsigned long)g->id, g->private_mode ? 1 : 0);
    safe_send(self->fd, out);
    return 0;
}

// Dispatcher
int server_dispatch_command(server_state_t *state, client_t *self, client_t *clients, const char *line_in)
{
    if (!state || !self || !line_in)
        return -1;
    char line[PROTO_MAX_LINE];
    strncpy(line, line_in, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    char *args = NULL;
    const char *cmd = parse_cmd(line, &args);

    // If in replay mode, only allow replay commands
    if (self->in_replay_mode) {
        if (strcmp(cmd, CMD_REPLAY_NEXT) == 0) return handle_replay_next(state, self, clients, args);
        if (strcmp(cmd, CMD_REPLAY_PREV) == 0) return handle_replay_prev(state, self, clients, args);
        if (strcmp(cmd, CMD_REPLAY_EXIT) == 0) return handle_replay_exit(state, self, clients, args);
        safe_send(self->fd, "In replay mode. Use: next | previous | exit\n");
        return 0;
    }

    if (strcmp(cmd, CMD_REGISTER) == 0) return handle_register(state, self, clients, args);
    if (strcmp(cmd, CMD_BIO) == 0) return handle_bio(state, self, clients, args);
    if (strcmp(cmd, CMD_BIO_SHOW) == 0) return handle_bio_show(state, self, clients, args);
    if (strcmp(cmd, CMD_LIST_USERS) == 0) return handle_list_users(state, self, clients, args);
    if (strcmp(cmd, CMD_CHALLENGE) == 0) return handle_challenge(state, self, clients, args);
    if (strcmp(cmd, CMD_ACCEPT) == 0) return handle_accept(state, self, clients, args);
    if (strcmp(cmd, CMD_REFUSE) == 0) return handle_refuse(state, self, clients, args);
    if (strcmp(cmd, CMD_CHAT) == 0) return handle_chat(state, self, clients, args);
    if (strcmp(cmd, CMD_GROUP_CREATE) == 0) return handle_group_create(state, self, clients, args);
    if (strcmp(cmd, CMD_GROUP_INVITE) == 0) return handle_group_invite(state, self, clients, args);
    if (strcmp(cmd, CMD_GROUP_QUIT) == 0) return handle_group_quit(state, self, clients, args);
    if (strcmp(cmd, CMD_FRIEND_ADD) == 0) return handle_friend_add(state, self, clients, args);
    if (strcmp(cmd, CMD_FRIEND_REMOVE) == 0) return handle_friend_remove(state, self, clients, args);
    if (strcmp(cmd, CMD_LIST_FRIENDS) == 0) return handle_list_friends(state, self, clients, args);
    if (strcmp(cmd, CMD_MOVE) == 0) return handle_move(state, self, clients, args);
    if (strcmp(cmd, CMD_SHOW_GAMES) == 0) return handle_show_games(state, self, clients, args);
    if (strcmp(cmd, CMD_LIST_GAMES) == 0) return handle_list_games(state, self, clients, args);
    if (strcmp(cmd, CMD_OBSERVE) == 0) return handle_observe(state, self, clients, args);
    if (strcmp(cmd, CMD_STOP_OBSERVE) == 0) return handle_stop_observe(state, self, clients, args);
    if (strcmp(cmd, CMD_SET_PRIVATE) == 0) return handle_set_private(state, self, clients, args);
    if (strcmp(cmd, CMD_FOCUS) == 0) return handle_focus(state, self, clients, args);
    if (strcmp(cmd, CMD_SHOW_BOARD) == 0) return handle_show_board(state, self, clients, args);
    if (strcmp(cmd, CMD_GET_REPLAY) == 0) return handle_get_replay(state, self, clients, args);

    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ECHO %s\n", line_in);
    safe_send(self->fd, resp);
    return 0;
}
