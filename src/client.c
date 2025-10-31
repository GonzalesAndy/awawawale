#define _POSIX_C_SOURCE 200112L
#include "client.h"
#include "client_cli.h"
#include "client_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

/* Convenience wrappers matching prototypes in client.h */
int client_send_register(int sockfd, const char *username)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_register(out, sizeof(out), username))
        return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}
int client_send_challenge(int sockfd, const char *from, const char *to)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_challenge(out, sizeof(out), from, to))
        return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}

static void reprint_prompt(const char *username)
{
    client_cli_print_prompt(username);
}

int client_run(void)
{
    char username[64] = "";
    int sockfd = -1;
    char line[PROTO_MAX_LINE];

    printf("Awalé CLI client. Type 'help' for commands.\n");
    client_cli_print_help();
    reprint_prompt(username);

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

        if (sockfd != -1 && FD_ISSET(sockfd, &readfds))
        {
            char in[PROTO_MAX_LINE];
            ssize_t r = recv(sockfd, in, sizeof(in) - 1, 0);
            if (r > 0)
            {
                in[r] = '\0';
                {
                    char *p = in;
                    while (*p)
                    {
                        char *line = p;
                        char *nl = strchr(line, '\n');
                        if (nl)
                            *nl = '\0';
                        if (strncmp(line, "REGISTERED ", 11) == 0)
                        {
                            const char *name = line + 11;
                            while (*name == ' ')
                                ++name;
                            if (*name)
                            {
                                strncpy(username, name, sizeof(username) - 1);
                                username[sizeof(username) - 1] = '\0';
                            }
                        }
                        if (!nl)
                            break;
                        p = nl + 1;
                    }
                }
                printf("\r\x1b[2K");
                printf("%sServer:%s %s\n", "\x1b[35m", "\x1b[0m", in);
                reprint_prompt(username);
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

        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            if (!fgets(line, sizeof(line), stdin))
                break;
            size_t len = strlen(line);
            if (len && line[len - 1] == '\n')
                line[len - 1] = '\0';
            if (strlen(line) == 0)
            {
                printf("\x1b[1A\x1b[2K");
                reprint_prompt(username);
                continue;
            }

            // Inline handle connect as a local action
            if (strncmp(line, "connect ", 8) == 0)
            {
                char host[256] = {0}, port[32] = {0};
                if (sscanf(line + 8, "%255s %31s", host, port) == 2)
                {
                    int fd = client_io_connect(host, port);
                    if (fd == -1)
                        printf("Failed to connect\n");
                    else
                    {
                        if (sockfd != -1)
                            close(sockfd);
                        sockfd = fd;
                        printf("Connected to %s:%s (fd=%d)\r\n", host, port, fd);
                    }
                }
                reprint_prompt(username);
                continue;
            }

            char out[PROTO_MAX_LINE];
            bool send_out = client_cli_handle_input(username, line, out, &sockfd);
            if (send_out && out[0] != '\0' && sockfd != -1)
            {
                ssize_t s = send(sockfd, out, strlen(out), 0);
                if (s < 0)
                    perror("send");
            }
            else
            {
                if (strcmp(line, "quit") == 0)
                    break;
                reprint_prompt(username);
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
    (void)argc;
    (void)argv;
    return client_run();
}
