#define _POSIX_C_SOURCE 200112L
#include "client.h"
#include <stdio.h>
#include "game.h"

#include "protocol.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>
#include <ctype.h>
#include <time.h>

static void print_help(void)
{
    printf("Commands:\n");
    printf("  connect <host> <port> - connect to server\n");
    printf("  register <name>       - set your username\n");
    printf("  bio <text>            - set your bio\n");
    printf("  show_bio <username>   - show a user's bio, if <username> empty show your own\n");
    printf("  list                  - request list of users\n");
    printf("  challenge <user>      - challenge a user\n");
    printf("  accept <user>         - accept a challenge\n");
    printf("  refuse <user>         - refuse a challenge\n");
    printf("  move <pit>            - play a move (pit index 0..11)\n");
    printf("  chat <message>        - send chat message\n");
    printf("  help                  - show this help\n");
    printf("  quit                  - exit\n\n\n");
}

/* Print the interactive prompt. If the client is in a game, show the mode and
 * game id so the user knows they are playing. Uses ANSI colors when available.
 */
static int client_in_game = 0;
static uint64_t client_game_id = 0;
static int client_is_my_turn = 0; /* whether it's this client's turn */
static void print_prompt(const char *username)
{
    const char *GREEN = "\x1b[32m";
    const char *CYAN = "\x1b[36m";
    const char *YELLOW = "\x1b[33m";
    const char *RESET = "\x1b[0m";
    if (client_in_game) {
        if (username && username[0] != '\0')
            printf("%sawalé%s %s%s%s (%sPLAY %lu%s)%s> ", CYAN, RESET, GREEN, username, RESET, YELLOW, (unsigned long)client_game_id, RESET, client_is_my_turn?" [YOUR TURN]":"");
        else
            printf("%sawalé%s (%sPLAY %lu%s)%s> ", CYAN, RESET, YELLOW, (unsigned long)client_game_id, RESET, client_is_my_turn?" [YOUR TURN]":"");
    } else {
        if (username && username[0] != '\0')
            printf("%sawalé%s %s%s%s> ", CYAN, RESET, GREEN, username, RESET);
        else
            printf("%sawalé%s > ", CYAN, RESET);
    }
    fflush(stdout);
}

/* local canonicalize helper: lower-case and convert '_'->' ' to compare names robustly */
static void canonicalize_local(char *out, const char *in, size_t n) {
    if (!out || n == 0) return;
    size_t w = 0;
    for (size_t i = 0; in && in[i] && w + 1 < n; ++i) {
        char c = in[i];
        if (c == '_') c = ' ';
        out[w++] = (char)tolower((unsigned char)c);
    }
    out[w] = '\0';
}

/* Helper: connect to host:port returning socket fd or -1 on error */
static int client_connect_to(const char *host, const char *port)
{
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0)
    {
        perror("getaddrinfo");
        return -1;
    }
    int sockfd = -1;
    struct addrinfo *rp;
    for (rp = res; rp; rp = rp->ai_next)
    {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
            continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);
    return sockfd;
}

/* Convenience wrappers matching prototypes in client.h */
int client_send_register(int sockfd, const char *username)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_register(out, sizeof(out), username)) return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}

int client_send_challenge(int sockfd, const char *from, const char *to)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_challenge(out, sizeof(out), from, to)) return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}

int client_send_move(int sockfd, uint64_t game_id, int pit_index)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_move(out, sizeof(out), "", game_id, pit_index)) return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}

int client_send_chat(int sockfd, const char *from, const char *to, const char *msg)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_chat(out, sizeof(out), from, to, msg)) return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}

int client_send_bio_set(int sockfd, const char *from, const char *bio_text)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_bio_set(out, sizeof(out), from, bio_text)) return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}

int client_send_bio_show(int sockfd, const char *requester, const char *target_username)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_bio_show(out, sizeof(out), requester, target_username)) return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}

/* Process a single incoming line from the user and write protocol output
 * into `out`. If a connect command is issued, *sockfd may be updated.
 * Returns true if `out` should be sent to server. */
static bool client_handle_input(const char *username, const char *line_in, char *out, int *sockfd)
{
    char tmp[PROTO_MAX_LINE];
    strncpy(tmp, line_in, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *cmd = strtok(tmp, " ");
    if (!cmd) return false;
    out[0] = '\0';
    if (strcmp(cmd, "help") == 0)
    {
        print_help();
        return false;
    }
    else if (strcmp(cmd, "quit") == 0)
    {
        return false; /* caller will exit */
    }
    else if (strcmp(cmd, "connect") == 0)
    {
        char *host = strtok(NULL, " ");
        char *port = strtok(NULL, " ");
        if (!host || !port)
        {
            printf("Usage: connect <host> <port>\n");
            return false;
        }
        int fd = client_connect_to(host, port);
        if (fd == -1)
        {
            printf("Failed to connect\n");
        }
        else
        {
            if (*sockfd != -1)
                close(*sockfd);
            *sockfd = fd;
            printf("Connected to %s:%s (fd=%d)\r\n", host, port, fd);
        }
        return false;
    }
    else if (strcmp(cmd, "register") == 0)
    {
        char *name = strtok(NULL, "");
        if (!name)
        {
            printf("Usage: register <name>\n");
            return false;
        }
        if (*sockfd == -1)
        {
            printf("Not connected to a server.\r\n");
            return false;
        }
        proto_build_register(out, PROTO_MAX_LINE, name);
        return true;
    }
    else if (strcmp(cmd, "bio") == 0)
    {
        char *bio_text = strtok(NULL, "");
        if (!bio_text)
        {
            printf("Usage: bio <text>\n");
            return false;
        }
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        proto_build_bio_set(out, PROTO_MAX_LINE, username, bio_text);
        return true;

    }
    else if (strcmp(cmd, "show_bio") == 0)
    {
        char *target_username = strtok(NULL, "");
        if (!target_username)
        {
            target_username = (char *)username; /* show own bio if no username given */
        }
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        proto_build_bio_show(out, PROTO_MAX_LINE, username, target_username);
        return true;
    }
    else if (strcmp(cmd, "list") == 0)
    {
        proto_build_list_users(out, PROTO_MAX_LINE);
        return true;
    }
    else if (strcmp(cmd, "challenge") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *user = strtok(NULL, " ");
        if (!user)
        {
            printf("Usage: challenge <user>\n");
            return false;
        }
        proto_build_challenge(out, PROTO_MAX_LINE, username, user);
        return true;
    }
    else if (strcmp(cmd, "accept") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *user = strtok(NULL, " ");
        if (!user)
        {
            printf("Usage: accept <user>\n");
            return false;
        }
        proto_build_accept(out, PROTO_MAX_LINE, username, user);
        return true;
    }
    else if (strcmp(cmd, "refuse") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *user = strtok(NULL, " ");
        if (!user)
        {
            printf("Usage: refuse <user>\n");
            return false;
        }
        proto_build_refuse(out, PROTO_MAX_LINE, username, user);
        return true;
    }
    else if (strcmp(cmd, "move") == 0)
    {
        /* move <pit>  (when already in a game) OR move <game-id> <pit> */
        if (username[0] == '\0') {
            printf("You must register a username first.\n");
            return false;
        }
        char *a = strtok(NULL, " ");
        if (!a) { printf("Usage: move <pit> OR move <game-id> <pit>\n"); return false; }
        char *b = strtok(NULL, " ");
        uint64_t gid = 0;
        int pit = 0;
        if (b) {
            gid = (uint64_t)strtoull(a, NULL, 10);
            pit = atoi(b);
        } else {
            if (!client_in_game) { printf("Not currently in a game; specify game id: move <game-id> <pit>\n"); return false; }
            gid = client_game_id;
            pit = atoi(a);
        }
        /* Build MOVE message with username */
        if (!proto_build_move(out, PROTO_MAX_LINE, username, gid, pit)) return false;
        /* mark as waiting until MOVE_OK or next GAME_UPDATE */
        client_is_my_turn = 0;
        return true;
    }
    else
    {
        printf("Unknown command '%s'\n", cmd);
        return false;
    }
}

/* Run the client loop. Returns when quitting. */
int client_run(void)
{
    char username[64] = "";
    int sockfd = -1;
    char line[PROTO_MAX_LINE];

    printf("Awalé CLI client. Type 'help' for commands.\n");
    print_help();
        /* initial prompt */
        print_prompt(username);

    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int maxfd = STDIN_FILENO;

        if (sockfd != -1)
        {
            FD_SET(sockfd, &readfds);
            if (sockfd > maxfd)
                maxfd = sockfd;
        }

        int sel = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (sel < 0)
        {
            perror("select");
            break;
        }

        /* Handle server messages */
        if (sockfd != -1 && FD_ISSET(sockfd, &readfds))
        {
            char in[PROTO_MAX_LINE];
            ssize_t r = recv(sockfd, in, sizeof(in) - 1, 0);
            if (r > 0)
            {
                in[r] = '\0';
                /* Handle potentially multiple lines in `in` */
                char *saveptr = NULL;
                char *ln = strtok_r(in, "\n", &saveptr);
                while (ln) {
                    char *args = NULL;
                    /* parse command */
                    char tmp[PROTO_MAX_LINE];
                    strncpy(tmp, ln, sizeof(tmp)-1);
                    tmp[sizeof(tmp)-1] = '\0';
                    char *cmd = proto_parse_command(tmp, &args);

                    /* Clear current prompt line before printing an async message */
                    printf("\r\x1b[2K"); /* carriage return + clear line */

                    if (cmd && strcmp(cmd, CMD_GAME_UPDATE) == 0 && args) {
                        /* proto: GAME_UPDATE <game_id> <board_text>
                         * Use a local copy of board_text and strtok_r to avoid
                         * clobbering tmp or interfering with other tokenizers.
                         */
                        char *gid_s = args;
                        char *space = strchr(args, ' ');
                        char *board_text = NULL;
                        if (space) { *space = '\0'; board_text = space + 1; }
                        if (board_text && board_text[0] != '\0') {
                            printf("%sGame update:%s (game %s)\n", "\x1b[35m", "\x1b[0m", gid_s ? gid_s : "?");
                            game_t g;
                            memset(&g, 0, sizeof(g));

                            /* make a safe copy of board_text to tokenize */
                            char bt_copy[PROTO_MAX_LINE];
                            strncpy(bt_copy, board_text, sizeof(bt_copy)-1);
                            bt_copy[sizeof(bt_copy)-1] = '\0';

                            char *save = NULL;
                            char *tok = strtok_r(bt_copy, " ", &save);
                            if (tok) { g.id = (uint64_t)strtoull(tok, NULL, 10); tok = strtok_r(NULL, " ", &save); }
                            if (tok) {
                                if (tok[0] == 'A') g.turn = PLAYER_A;
                                else if (tok[0] == 'B') g.turn = PLAYER_B;
                                else g.turn = PLAYER_A;
                                tok = strtok_r(NULL, " ", &save);
                            }
                            if (tok) { g.score[0] = atoi(tok); tok = strtok_r(NULL, " ", &save); }
                            if (tok) { g.score[1] = atoi(tok); tok = strtok_r(NULL, " ", &save); }
                            for (int i = 0; i < N_PITS && tok; ++i) { g.pits[i] = atoi(tok); tok = strtok_r(NULL, " ", &save); }

                            /* consume moves_len and state if present */
                            if (tok) { /* moves_len */ tok = strtok_r(NULL, " ", &save); }
                            if (tok) { /* state */ tok = strtok_r(NULL, " ", &save); }

                            char *last_a = NULL, *last_b = NULL;
                            while (tok) { last_a = last_b; last_b = tok; tok = strtok_r(NULL, " ", &save); }
                            if (last_a) {
                                strncpy(g.player_name[0], last_a, GAME_MAX_USERNAME-1);
                                g.player_name[0][GAME_MAX_USERNAME-1] = '\0';
                                for (size_t i = 0; g.player_name[0][i]; ++i) if (g.player_name[0][i]=='_') g.player_name[0][i]=' ';
                            }
                            if (last_b) {
                                strncpy(g.player_name[1], last_b, GAME_MAX_USERNAME-1);
                                g.player_name[1][GAME_MAX_USERNAME-1] = '\0';
                                for (size_t i = 0; g.player_name[1][i]; ++i) if (g.player_name[1][i]=='_') g.player_name[1][i]=' ';
                            }

                            /* mark in-game and set id */
                            if (gid_s) { client_in_game = 1; client_game_id = (uint64_t)strtoull(gid_s, NULL, 10); }

                            /* print using game_print ASCII path */
                            game_print(&g, NULL, 0);

                            /* Decide if it's this client's turn; use canonicalized compare */
                            if (username[0] != '\0') {
                                char can_u[GAME_MAX_USERNAME];
                                char can_a[GAME_MAX_USERNAME];
                                char can_b[GAME_MAX_USERNAME];
                                canonicalize_local(can_u, username, sizeof(can_u));
                                canonicalize_local(can_a, g.player_name[0], sizeof(can_a));
                                canonicalize_local(can_b, g.player_name[1], sizeof(can_b));
                                if ((g.turn == PLAYER_A && strcmp(can_u, can_a) == 0) ||
                                    (g.turn == PLAYER_B && strcmp(can_u, can_b) == 0)) {
                                    client_is_my_turn = 1;
                                } else {
                                    client_is_my_turn = 0;
                                }
                            }
                        } else {
                            printf("%sServer:%s %s\n", "\x1b[35m", "\x1b[0m", ln);
                        }
                    } else if (cmd && strcmp(cmd, CMD_GAME_START_AT) == 0 && args) {
                        /* proto: GAME_START_AT <game_id> <start_ts> */
                        char *gid_s = strtok(args, " ");
                        char *ts_s = strtok(NULL, " ");
                        if (ts_s) {
                            time_t start = (time_t)strtoull(ts_s, NULL, 10);
                            time_t now = time(NULL);
                            long diff = (long)difftime(start, now);
                            if (diff > 0) {
                                printf("%sGame %s will start in %ld second(s)%s\n",
                                       "\x1b[33m", gid_s?gid_s:"?", diff, "\x1b[0m");
                            } else {
                                printf("%sGame %s starting now%s\n", "\x1b[33m", gid_s?gid_s:"?", "\x1b[0m");
                            }
                        } else {
                            printf("%sServer:%s %s\n", "\x1b[35m", "\x1b[0m", ln);
                        }
                    } else if (cmd && strcmp(cmd, "ACCEPTED") == 0 && args) {
                        /* Server informed us a challenge was accepted: ACCEPTED <acceptor> <challenger> <gid>
                         * If we're one of the players, mark as in-game so shorthand moves work. */
                        char *acceptor = strtok(args, " ");
                        char *challenger = strtok(NULL, " ");
                        char *gid_s = strtok(NULL, " \n");
                        if (gid_s) {
                            uint64_t gid = (uint64_t)strtoull(gid_s, NULL, 10);
                            if (acceptor && challenger) {
                                /* match our username (case-sensitive for now) */
                                if (username[0] != '\0' && (strcmp(username, acceptor) == 0 || strcmp(username, challenger) == 0)) {
                                    client_in_game = 1;
                                    client_game_id = gid;
                                    printf("%sNow in PLAY mode for game %lu (accepted) %s\n", "\x1b[33m", (unsigned long)gid, "\x1b[0m");
                                }
                            }
                        }
                    } else {
                        /* fallback: unknown command or plain server text */
                        printf("%sServer:%s %s\n", "\x1b[35m", "\x1b[0m", ln);
                    }

                    print_prompt(username);
                    ln = strtok_r(NULL, "\n", &saveptr);
                }
            }
            else if (r == 0)
            {
                printf("\nServer closed connection\n");
                close(sockfd);
                sockfd = -1;
            }
            else
            {
                perror("recv");
            }
        }

        /* Handle user input */
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            if (!fgets(line, sizeof(line), stdin))
                break;

            size_t len = strlen(line);
            if (len && line[len - 1] == '\n')
                line[len - 1] = '\0';
            if (strlen(line) == 0)
            {
                /* User pressed Enter on an empty line. The terminal already
                 * emitted a blank line; remove it and reprint the prompt so
                 * the UI stays compact. */
                printf("\x1b[1A\x1b[2K"); /* move cursor up and clear the line */
                print_prompt(username);
                continue;
            }

            char out[PROTO_MAX_LINE];
            bool send_out = client_handle_input(username, line, out, &sockfd);

            if (send_out && out[0] != '\0' && sockfd != -1)
            {
                if (strncmp(out, CMD_REGISTER, strlen(CMD_REGISTER)) == 0)
                {
                    char tmp[PROTO_MAX_LINE];
                    strncpy(tmp, out, sizeof(tmp) - 1);
                    tmp[sizeof(tmp) - 1] = '\0';
                    char *name = strchr(tmp, ' ');
                    if (name)
                    {
                        name++;
                        char *nl = strchr(name, '\n');
                        if (nl) *nl = '\0';
                        if (client_send_register(sockfd, name) < 0)
                            perror("send");
                        else
                        {
                            strncpy(username, name, sizeof(username) - 1);
                            username[sizeof(username) - 1] = '\0';
                        }
                    }
                }
                else if (strncmp(out, CMD_CHALLENGE, strlen(CMD_CHALLENGE)) == 0)
                {
                    char tmp[PROTO_MAX_LINE];
                    strncpy(tmp, out, sizeof(tmp) - 1);
                    tmp[sizeof(tmp) - 1] = '\0';
                    (void)strtok(tmp, " ");
                    char *from = strtok(NULL, " ");
                    char *to = strtok(NULL, " \n");
                    if (from && to)
                    {
                        if (client_send_challenge(sockfd, from, to) < 0)
                            perror("send");
                    }
                }
                else
                {
                    ssize_t s = send(sockfd, out, strlen(out), 0);
                    if (s < 0)
                        perror("send");
                }
            }
            else if (!send_out)
            {
                if (strcmp(line, "quit") == 0)
                {
                    break;
                }
                /* A local command produced output (e.g. Connected / Not connected)
                 * — reprint the prompt so the user can continue typing without
                 * having to press Enter. */
                print_prompt(username);
            }
        }
    }

    if (sockfd != -1)
        close(sockfd);

    printf("Client exiting.\n");
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    return client_run();
}
