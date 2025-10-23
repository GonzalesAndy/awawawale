#define _POSIX_C_SOURCE 200112L
#include "server.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 128
#define ADDRESS_PORT 9987

typedef struct
{
    int fd;
    char name[64];
    char buf[PROTO_MAX_LINE];
    size_t buf_len;
} client_t;

static void client_init(client_t *c)
{
    c->fd = -1;
    c->name[0] = '\0';
    c->buf_len = 0;
}

static int create_and_bind(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4 TCP socket
    if (sfd < 0)
        return -1;
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // allow reuse address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr)); // zero it
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) // bind to all interfaces
    {
        close(sfd);
        return -1;
    }
    if (listen(sfd, 16) < 0) // max 16 pending connections
    {
        close(sfd);
        return -1;
    }
    return sfd;
}

int main(int argc, char **argv)
{
    int port = ADDRESS_PORT;
    if (argc >= 2)
        port = atoi(argv[1]);
    int listen_fd = create_and_bind(port);
    if (listen_fd < 0)
    {
        perror("create_and_bind");
        return 1;
    }
    printf("Server listening on 0.0.0.0:%d\n", port);

    client_t clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; ++i)
        client_init(&clients[i]);

    fd_set readset;
    while (1)
    {
        FD_ZERO(&readset);
        FD_SET(listen_fd, &readset); // monitor listening socket
        int maxfd = listen_fd;
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i].fd != -1)
            {
                FD_SET(clients[i].fd, &readset);
                if (clients[i].fd > maxfd)
                    maxfd = clients[i].fd;
            }
        }

        int rv = select(maxfd + 1, &readset, NULL, NULL, NULL); // blocking wait for activity 
        if (rv < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (FD_ISSET(listen_fd, &readset)) // new incoming connection
        {
            struct sockaddr_in peer;
            socklen_t plen = sizeof(peer);
            int cfd = accept(listen_fd, (struct sockaddr *)&peer, &plen);
            if (cfd >= 0)
            {
                // add to clients
                int added = 0;
                for (int i = 0; i < MAX_CLIENTS; ++i)
                {
                    if (clients[i].fd == -1)
                    {
                        clients[i].fd = cfd;
                        clients[i].buf_len = 0;
                        clients[i].name[0] = '\0';
                        added = 1;
                        break;
                    }
                }
                if (!added)
                {
                    close(cfd);
                }
                else
                    printf("Accepted connection fd=%d\n", cfd);
            }
        }

        // handle client data
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            client_t *c = &clients[i];
            if (c->fd == -1)
                continue;
            if (!FD_ISSET(c->fd, &readset)) // no data
                continue;

            char tmp[PROTO_MAX_LINE];
            ssize_t r = recv(c->fd, tmp, sizeof(tmp) - 1, 0); // read data
            if (r > 0)
            {
                printf("DEBUG fd=%d received %zd bytes\n", c->fd, r);
            }
            if (r <= 0)
            {
                printf("Client fd=%d disconnected\n", c->fd);
                close(c->fd);
                client_init(c);
                continue;
            }
            tmp[r] = '\0';
            // append to buffer
            if (c->buf_len + (size_t)r >= sizeof(c->buf) - 1)
            {
                c->buf_len = 0;
            }
            memcpy(c->buf + c->buf_len, tmp, r); // append data to buffer
            c->buf_len += r;
            c->buf[c->buf_len] = '\0';

            // process complete lines
            char *line = c->buf;
            while (1)
            {
                char *nl = strchr(line, '\n'); // find newline
                if (!nl)
                    break;
                *nl = '\0';
                char *args = NULL;
                printf("Client fd=%d sent line: %s\n", c->fd, line);
                char *cmd = proto_parse_command(line, &args);
                if (!cmd)
                    cmd = line;
                // handle commands
                if (strcmp(cmd, CMD_REGISTER) == 0)
                {
                    // register username
                }
                else if (strcmp(cmd, CMD_LIST_USERS) == 0)
                {
                    // send user list
                }
                else
                {
                    char resp[PROTO_MAX_LINE];
                    snprintf(resp, sizeof(resp), "ECHO %s\n", line);
                    send(c->fd, resp, strlen(resp), 0);
                }

                // move to next line
                line = nl + 1;
            }
            // compact any remaining partial data
            size_t rem = strlen(line);
            if (rem > 0)
                memmove(c->buf, line, rem + 1);
            else
                c->buf_len = 0;
            c->buf_len = rem;
        }
    }

    close(listen_fd);
    return 0;
}
