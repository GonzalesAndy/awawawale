#define _POSIX_C_SOURCE 200112L
#include "client.h"
#include <stdio.h>

#include "protocol.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>

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

    /* Print the interactive prompt. Uses ANSI colors when available and keeps the
     * prompt compact so it can be reprinted after asynchronous server messages. */
    static void print_prompt(const char *username)
    {
        const char *GREEN = "\x1b[32m";
        const char *CYAN = "\x1b[36m";
        const char *RESET = "\x1b[0m";
        if (username && username[0] != '\0')
            printf("%sawalé%s %s%s%s> ", CYAN, RESET, GREEN, username, RESET);
        else
            printf("%sawalé%s > ", CYAN, RESET);
        fflush(stdout);
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


uint64_t client_get_next_game_id(client_state_t *cs, uint64_t user_id)
{
    (void)user_id; // unused for now
    uint64_t max_id = 0;
    for (int i = 0; i < cs->games_count; ++i)
    {
        if (cs->games[i].id > max_id)
            max_id = cs->games[i].id;
    }
    return max_id + 1;
}
uint64_t client_get_first_game_id(client_state_t *cs){
    if (!cs || cs->games_count == 0) return 0;
    /* Return the first game id whose state indicates an ongoing/playing
     * game. If none found return 0. This walks only the active entries
     * up to cs->games_count for efficiency and correctness. */
    for (int i = 0; i < cs->games_count; ++i) {
        if (cs->games[i].id != 0 && cs->games[i].state == GAME_STATE_ONGOING) {
            return cs->games[i].id;
        }
    }
    return 0;
}
game_t* client_get_game_by_id(client_state_t *cs, uint64_t game_id)
{
    for (int i = 0; i < cs->games_count; ++i)
    {
        if (cs->games[i].id == game_id)
            return &cs->games[i];
    }
    return NULL;
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
      //TODO implement the manner to make a moove
      
    }
    else if (strcmp(cmd, "chat") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *msg = strtok(NULL, "");
        if (!msg)
        {
            printf("Usage: chat <message>\n");
            return false;
        }
        proto_build_chat(out, PROTO_MAX_LINE, username, "", msg); // broadcast chat
        return true;
    }
    else
    {
        printf("Unknown command '%s'\n", cmd);
        return false;
    }
    return false;
}


/* Run the client loop. Returns when quitting. */
int client_run(void)
{
    char username[64] = "";
    int sockfd = -1;
    char line[PROTO_MAX_LINE];
    /* local client-side games list */
    game_t client_games[16];
    int client_games_count = 0;

    printf("Awalé CLI client. Type 'help' for commands.\n");
    print_help();
        /* initial prompt */
        print_prompt(username);

    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        uint64_t current_game_id = client_get_first_game_id(NULL);
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
                /* If server notifies that a challenge was accepted, create a
                 * local game record for it. Expected format (server):
                 * "ACCEPTED, THE GAME WILL START SOON <acceptor> <challenger> <gid>\n"
                 */
                if (strstr(in, "ACCEPTED")) {
                    char a[GAME_MAX_USERNAME];
                    char b[GAME_MAX_USERNAME];
                    unsigned long gid = 0;
                    int sc = sscanf(in, "ACCEPTED, THE GAME WILL START SOON %31s %31s %lu", a, b, &gid);
                    if (sc == 3) {
                        int found = 0;
                        for (int i = 0; i < client_games_count; ++i) {
                            if (client_games[i].id == (uint64_t)gid) { found = 1; break; }
                        }
                        if (!found && client_games_count < (int)(sizeof(client_games)/sizeof(client_games[0]))) {
                            game_init(&client_games[client_games_count], a, b);
                            client_games[client_games_count].id = (uint64_t)gid;
                            current_game_id = (uint64_t)gid;
                            //DEBUG
                            printf("current_game_id set to %lu\n", current_game_id);
                            //DEBUG
                            client_games_count++;
                            printf("\r\x1b[2K");
                            printf("%sServer:%s ACCEPTED -> created local game %lu (%s vs %s)\n", "\x1b[35m", "\x1b[0m", gid, a, b);
                            
                        }
                    }
                }

                in[r] = '\0';
                /* Clear current prompt line so we don't leave an empty prompt
                 * above the server message; then print the server message and
                 * reprint the prompt. */
                printf("\r\x1b[2K"); /* carriage return + clear line */
                printf("%sServer:%s %s\n", "\x1b[35m", "\x1b[0m", in);
                print_prompt(username);
                //si on est dans une pertie on met le nom de partie en jaune apres le nom d'utilisateur
                if (current_game_id != 0) {
                    printf("\x1b[33m[Game %lu]\x1b[0m ", current_game_id);
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
