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
#include <stdbool.h>

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
            char buf[PROTO_MAX_LINE];
            ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);
            if (n <= 0)
            {
                if (n == 0)
                    printf("\nServer closed connection\n");
                else
                    perror("recv");
                close(sockfd);
                sockfd = -1;
                continue;
            }

            buf[n] = '\0';

            char *lineptr = strtok(buf, "\n");
            while (lineptr)
            {
                while (*lineptr == ' ')
                    ++lineptr;

                if (strncmp(lineptr, "REGISTERED ", 11) == 0)
                {
                    const char *name = lineptr + 11;
                    while (*name == ' ')
                        ++name;
                    if (*name)
                    {
                        strncpy(username, name, sizeof(username) - 1);
                        username[sizeof(username) - 1] = '\0';
                    }
                }

                printf("\r\x1b[2K%sServer:%s %s\n", "\x1b[35m", "\x1b[0m", lineptr);
                lineptr = strtok(NULL, "\n");
            }

            reprint_prompt(username);
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
                        printf("Connected to %s:%s (fd=%d)\n", host, port, fd);
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
