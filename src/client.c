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
int client_send_move(int sockfd, uint64_t game_id, int pit_index)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_move(out, sizeof(out), "", game_id, pit_index))
        return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}
int client_send_chat(int sockfd, const char *from, const char *to, const char *msg)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_chat(out, sizeof(out), from, to, msg))
        return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}
int client_send_bio_set(int sockfd, const char *from, const char *bio_text)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_bio_set(out, sizeof(out), from, bio_text))
        return -1;
    return (int)send(sockfd, out, strlen(out), 0);
}
int client_send_bio_show(int sockfd, const char *requester, const char *target_username)
{
    char out[PROTO_MAX_LINE];
    if (!proto_build_bio_show(out, sizeof(out), requester, target_username))
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
                        if (nl)
                            *nl = '\0';
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
