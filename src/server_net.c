#define _POSIX_C_SOURCE 200112L
#include "server_net.h"
#include "server_handlers.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

static void client_init(client_t *c)
{
    c->fd = -1;
    c->name[0] = '\0';
    c->buf_len = 0;
    c->buf[0] = '\0';
}

static int create_and_bind(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
        return -1;
    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        perror("setsockopt(SO_REUSEADDR)");
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sfd);
        return -1;
    }
    if (listen(sfd, 16) < 0)
    {
        close(sfd);
        return -1;
    }
    return sfd;
}

static void server_accept_new(int listen_fd, client_t *clients, int max_clients)
{
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    int cfd = accept(listen_fd, (struct sockaddr *)&peer, &plen);
    if (cfd >= 0)
    {
        int added = 0;
        for (int i = 0; i < max_clients; ++i)
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
            close(cfd);
        else
            printf("Accepted connection fd=%d\n", cfd);
    }
}

static void server_handle_client_data(client_t *c, client_t *clients, server_state_t *state)
{
    char tmp[PROTO_MAX_LINE];
    ssize_t r = recv(c->fd, tmp, sizeof(tmp) - 1, 0);
    if (r <= 0)
    {
        if (r == 0)
            printf("Client fd=%d disconnected\n", c->fd);
        else
            perror("recv");
        close(c->fd);
        if (c->name[0] != '\0')
            server_mark_user_offline(state, c->name);
        client_init(c);
        return;
    }
    tmp[r] = '\0';

    if (c->buf_len + (size_t)r >= sizeof(c->buf) - 1)
        c->buf_len = 0; // overflow: reset buffer
    memcpy(c->buf + c->buf_len, tmp, r);
    c->buf_len += (size_t)r;
    c->buf[c->buf_len] = '\0';

    char *line = c->buf;
    while (1)
    {
        char *nl = strchr(line, '\n');
        if (!nl)
            break;
        *nl = '\0';
        printf("Client fd=%d sent line: %s\n", c->fd, line);
        server_dispatch_command(state, c, clients, line);
        line = nl + 1;
    }
    size_t rem = strlen(line);
    if (rem > 0)
        memmove(c->buf, line, rem + 1);
    else
        c->buf[0] = '\0';
    c->buf_len = rem;
}

int server_run(int port)
{
    int listen_fd = create_and_bind(port);
    if (listen_fd < 0)
    {
        perror("create_and_bind");
        return 1;
    }
    printf("Server listening on 0.0.0.0:%d\n", port);

    client_t clients[SERVER_NET_MAX_CLIENTS];
    for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
        client_init(&clients[i]);

    server_state_t state;
    server_init(&state);

    fd_set readset;
    while (1)
    {
        FD_ZERO(&readset);
        FD_SET(listen_fd, &readset);
        int maxfd = listen_fd;
        for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
        {
            if (clients[i].fd != -1)
            {
                FD_SET(clients[i].fd, &readset);
                if (clients[i].fd > maxfd)
                    maxfd = clients[i].fd;
            }
        }
        int rv = select(maxfd + 1, &readset, NULL, NULL, NULL);
        if (rv < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }
        if (FD_ISSET(listen_fd, &readset))
            server_accept_new(listen_fd, clients, SERVER_NET_MAX_CLIENTS);
        for (int i = 0; i < SERVER_NET_MAX_CLIENTS; ++i)
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &readset))
                server_handle_client_data(&clients[i], clients, &state);
    }

    close(listen_fd);
    return 0;
}
