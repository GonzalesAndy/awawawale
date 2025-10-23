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

static void print_help(void)
{
    printf("Commands:\n");
    printf("  register <name>      - set your username\n");
    printf("  connect <host> <port> - connect to server\n");
    printf("  list                 - request list of users\n");
    printf("  challenge <user>     - challenge a user\n");
    printf("  accept <user>        - accept a challenge\n");
    printf("  refuse <user>        - refuse a challenge\n");
    printf("  move <pit>           - play a move (pit index 0..11)\n");
    printf("  chat <message>       - send chat message\n");
    printf("  help                 - show this help\n");
    printf("  quit                 - exit\n");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char username[64] = "";
    int sockfd = -1;
    char line[PROTO_MAX_LINE];

    printf("Awalé CLI client. Type 'help' for commands.\n");
    print_help();

    while (1)
    {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin))
            break;
        // strip newline
        size_t line_length = strlen(line);
        if (line_length && line[line_length - 1] == '\n')
            line[line_length - 1] = '\0';
        if (strlen(line) == 0)
            continue;

        // parse words
        char *cmd = strtok(line, " ");
        if (!cmd)
            continue;

        if (strcmp(cmd, "help") == 0)
        {
            print_help();
            continue;
        }

        if (strcmp(cmd, "quit") == 0)
            break;

        if (strcmp(cmd, "connect") == 0)
        {
            char *host = strtok(NULL, " ");
            char *port = strtok(NULL, " ");
            if (!host || !port)
            {
                printf("Usage: connect <host> <port>\n");
                continue;
            }
            if (sockfd != -1)
            {
                close(sockfd);
                sockfd = -1;
            }

            struct addrinfo hints = {0}, *res = NULL;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(host, port, &hints, &res) != 0)
            {
                perror("getaddrinfo");
                continue;
            }
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
            if (sockfd == -1)
            {
                printf("Failed to connect\n");
                continue;
            }
            printf("Connected to %s:%s\n", host, port);
            continue;
        }

        // build a protocol message
        char out[PROTO_MAX_LINE];
        out[0] = '\0';

        if (strcmp(cmd, "list") == 0)
        {
            proto_build_list_users(out, sizeof(out));
        }
        else if (strcmp(cmd, "register") == 0)
        {
            char *name = strtok(NULL, "");
            if (!name)
            {
                printf("Usage: register <name>\n");
                continue;
            }
            if (sockfd != -1)
            {
                proto_build_register(out, sizeof(out), name);
                ssize_t s = send(sockfd, out, strlen(out), 0);
                if (s < 0)
                    perror("send");
                else {
                    strncpy(username, name, sizeof(username) - 1);
                    username[sizeof(username) - 1] = '\0';
                }
                char in[PROTO_MAX_LINE];
                ssize_t r = recv(sockfd, in, sizeof(in) - 1, 0);
                if (r > 0)
                {
                    in[r] = '\0';
                    printf("Server: %s\n", in);
                }
            }
            else
            {
                printf("Not connected to a server.\n");
            }
            continue; /* avoid falling through to unknown command / sending logic */
        }
        else if (strcmp(cmd, "challenge") == 0)
        {
            // challenge a user
        }
        else if (strcmp(cmd, "accept") == 0)
        {
            // accept a challenge
        }
        else if (strcmp(cmd, "refuse") == 0)
        {
            // refuse a challenge
        }
        else if (strcmp(cmd, "move") == 0)
        {
            // play a move
        }
        else if (strcmp(cmd, "chat") == 0)
        {
            // send chat message
        }
        else
        {
            printf("Unknown command '%s'\n", cmd);
            continue;
        }

        if (out[0] == '\0')
            continue;

        if (sockfd != -1)
        {
            // send to server
            size_t tosend = strlen(out);
            ssize_t s = send(sockfd, out, tosend, 0);
            if (s < 0)
                perror("send");
            else
                printf("Sent: %s\n", out);
            // try to read any incoming messages briefly
            char in[PROTO_MAX_LINE];
            ssize_t r = recv(sockfd, in, sizeof(in) - 1, 0);
            if (r > 0)
            {
                in[r] = '\0';
                printf("Server: %s\n", in);
            }
        }
        else
        {
            printf("Not connected to server. Cannot send message.\n");
        }
    }

    if (sockfd != -1)
        close(sockfd);
    printf("Client exiting.\n");
    return 0;
}
