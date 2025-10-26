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
    printf("  register <name>       - set your username\n");
    printf("  connect <host> <port> - connect to server\n");
    printf("  list                  - request list of users\n");
    printf("  challenge <user>      - challenge a user\n");
    printf("  accept <user>         - accept a challenge\n");
    printf("  refuse <user>         - refuse a challenge\n");
    printf("  move <pit>            - play a move (pit index 0..11)\n");
    printf("  chat <message>        - send chat message\n");
    printf("  help                  - show this help\n");
    printf("  quit                  - exit\n\n\n");
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
            printf("Connected to %s:%s (fd=%d)\n", host, port, fd);
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
            printf("Not connected to a server.\n");
            return false;
        }
        proto_build_register(out, PROTO_MAX_LINE, name);
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
                printf("\nServer: %s\n", in);
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
                continue;

            char out[PROTO_MAX_LINE];
            bool send_out = client_handle_input(username, line, out, &sockfd);

            if (send_out && out[0] != '\0' && sockfd != -1)
            {
                /* Prefer using high-level client_send_* wrappers where available */
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
                    char *tok = strtok(tmp, " ");
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
            else if (!send_out && strcmp(line, "quit") == 0)
            {
                break;
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
