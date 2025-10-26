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
    char name[GAME_MAX_USERNAME];
    char buf[PROTO_MAX_LINE];
    size_t buf_len;
} client_t;

/* Simple server API implementations backed by server_state_t */
int server_init(server_state_t *s)
{
    if (!s)
        return -1;
    s->users_count = 0;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        s->users[i].socket_fd = -1;
        s->users[i].username[0] = '\0';
    }
    s->games_count = 0;
    s->challenges_count = 0;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
        s->challenges[i].active = false;
    s->next_game_id = 1;
    return 0;
}

int server_register_user(server_state_t *s, const char *username, int sockfd)
{
    if (!s || !username)
        return -1;
    /* If user exists, update socket */
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' &&
            strcmp(s->users[i].username, username) == 0)
        {
            if (s->users[i].socket_fd != -1 && s->users[i].socket_fd != sockfd)
                return -1; /* username already in use by another connection */
            s->users[i].socket_fd = sockfd;
            return 0;
        }
    }
    /* Otherwise insert into first free slot */
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] == '\0')
        {
            strncpy(s->users[i].username, username, GAME_MAX_USERNAME - 1);
            s->users[i].username[GAME_MAX_USERNAME - 1] = '\0';
            s->users[i].socket_fd = sockfd;
            s->users_count++;
            return 0;
        }
    }
    return -1;
}

int server_set_user_bio(server_state_t *s, const char *username, const char *bio_text)
{
    if (!s || !username || !bio_text)
        return -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' &&
            strcmp(s->users[i].username, username) == 0)
        {
            strncpy(s->users[i].bio, bio_text, CLIENT_MAX_BIO - 1);
            s->users[i].bio[CLIENT_MAX_BIO - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

int server_show_user_bio(server_state_t *s, const char *requester, const char *target_username, char *out_bio, size_t n)
{
    if (!s || !requester || !target_username || !out_bio || n == 0)
        return -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' &&
            strcmp(s->users[i].username, target_username) == 0)
        {
            /* copy bio into caller buffer */
            strncpy(out_bio, s->users[i].bio, n - 1);
            out_bio[n - 1] = '\0';
            /* also log on server console */
            printf("Bio of %s:\n%s\n", target_username, s->users[i].bio);
            return 0;
        }
    }
    return -1;
}

int server_unregister_user(server_state_t *s, const char *username)
{
    if (!s || !username)
        return -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' &&
            strcmp(s->users[i].username, username) == 0)
        {
            s->users[i].username[0] = '\0';
            s->users[i].socket_fd = -1;
            if (s->users_count > 0)
                s->users_count--;
            return 0;
        }
    }
    return -1;
}

int server_list_online_users(server_state_t *s, char dest[][GAME_MAX_USERNAME], int max)
{
    if (!s || !dest || max <= 0)
        return 0;
    int n = 0;
    for (int i = 0; i < SERVER_MAX_USERS && n < max; ++i)
    {
        if (s->users[i].username[0] != '\0' && s->users[i].socket_fd != -1)
        {
            strncpy(dest[n], s->users[i].username, GAME_MAX_USERNAME - 1);
            dest[n][GAME_MAX_USERNAME - 1] = '\0';
            n++;
        }
    }
    return n;
}

int server_create_challenge(server_state_t *s, const char *from, const char *to)
{
    if (!s || !from || !to)
        return -1;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
    {
        if (!s->challenges[i].active)
        {
            strncpy(s->challenges[i].from, from, GAME_MAX_USERNAME - 1);
            s->challenges[i].from[GAME_MAX_USERNAME - 1] = '\0';
            strncpy(s->challenges[i].to, to, GAME_MAX_USERNAME - 1);
            s->challenges[i].to[GAME_MAX_USERNAME - 1] = '\0';
            s->challenges[i].active = true;
            s->challenges_count++;
            return 0;
        }
    }
    return -1;
}

int server_cancel_challenge(server_state_t *s, const char *from, const char *to)
{
    if (!s || !from || !to)
        return -1;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
    {
        if (s->challenges[i].active &&
            strcmp(s->challenges[i].from, from) == 0 &&
            strcmp(s->challenges[i].to, to) == 0)
        {
            s->challenges[i].active = false;
            if (s->challenges_count > 0)
                s->challenges_count--;
            return 0;
        }
    }
    return -1;
}

int server_accept_challenge(server_state_t *s, const char *from, const char *to, uint64_t *out_game_id)
{
    if (!s || !from || !to)
        return -1;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
    {
        if (s->challenges[i].active &&
            strcmp(s->challenges[i].from, from) == 0 &&
            strcmp(s->challenges[i].to, to) == 0)
        {
            s->challenges[i].active = false;
            if (s->challenges_count > 0)
                s->challenges_count--;
            if (out_game_id)
                *out_game_id = s->next_game_id++;
            return 0;
        }
    }
    return -1;
}

int server_refuse_challenge(server_state_t *s, const char *from, const char *to)
{
    return server_cancel_challenge(s, from, to);
}

/* --- socket helpers --- */
static int create_and_bind(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
        return -1;

    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        /* non-fatal: we continue but log to stderr */
        perror("setsockopt(SO_REUSEADDR)");
    }

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

static void client_init(client_t *c)
{
    c->fd = -1;
    c->name[0] = '\0';
    c->buf_len = 0;
}

/* Accept new connection and add to clients[] */
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

/* Handle a single client socket that has readable data. */
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
        client_init(c);
        return;
    }
    tmp[r] = '\0';

    /* Append to client buffer (simple bounded append) */
    if (c->buf_len + (size_t)r >= sizeof(c->buf) - 1)
        c->buf_len = 0; /* overflow: reset buffer */
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
        char *args = NULL;
        printf("Client fd=%d sent line: %s\n", c->fd, line);
        char *cmd = proto_parse_command(line, &args);
        if (!cmd)
            cmd = line;

        if (strcmp(cmd, CMD_REGISTER) == 0)
        {
            if (args)
            {
                char uname[GAME_MAX_USERNAME];
                strncpy(uname, args, sizeof(uname) - 1);
                uname[sizeof(uname) - 1] = '\0';
                /* If client already had a different name, unregister it first */
                if (c->name[0] != '\0' && strcmp(c->name, uname) != 0)
                {
                    server_unregister_user(state, c->name);
                }
                if (server_register_user(state, uname, c->fd) == 0)
                {
                    strncpy(c->name, uname, sizeof(c->name) - 1);
                    c->name[sizeof(c->name) - 1] = '\0';
                    char resp[PROTO_MAX_LINE];
                    snprintf(resp, sizeof(resp), "REGISTERED %s\n", c->name);
                    send(c->fd, resp, strlen(resp), 0);
                } else {
                    char resp[PROTO_MAX_LINE];
                    snprintf(resp, sizeof(resp), "ERROR username %s already in use\n", uname);
                    send(c->fd, resp, strlen(resp), 0);
                }
            }
        }
        else if (strcmp(cmd, CMD_BIO) == 0)
        {
            if (args)
            {
                char *username = strtok(args, " ");
                char *bio_text = strtok(NULL, "");
                if (username && bio_text)
                {
                    if (server_set_user_bio(state, username, bio_text) == 0)
                    {
                        char resp[PROTO_MAX_LINE];
                        snprintf(resp, sizeof(resp), "BIO_SET %s\n", username);
                        send(c->fd, resp, strlen(resp), 0);
                    }
                    else
                    {
                        char resp[PROTO_MAX_LINE];
                        snprintf(resp, sizeof(resp), "ERROR user %s not found\n", username);
                        send(c->fd, resp, strlen(resp), 0);
                    }
                }
            }
        }
        else if (strcmp(cmd, CMD_BIO_SHOW) == 0)
        {
            if (args)
            {
                /* proto: BIO_SHOW <requester> <target_username> */
                char *requester = strtok(args, " ");
                char *target_username = NULL;
                if (requester)
                    target_username = strtok(NULL, " \n");
                if (target_username)
                {
                    char biobuf[CLIENT_MAX_BIO];
                    if (server_show_user_bio(state, requester, target_username, biobuf, sizeof(biobuf)) == 0)
                    {
                        char resp[PROTO_MAX_LINE];
                        /* send username + bio on one line */
                        snprintf(resp, sizeof(resp), "BIO_SHOWN %s %s\n", target_username, biobuf);
                        send(c->fd, resp, strlen(resp), 0);
                    }
                    else
                    {
                        char resp[PROTO_MAX_LINE];
                        snprintf(resp, sizeof(resp), "ERROR user %s not found\n", target_username);
                        send(c->fd, resp, strlen(resp), 0);
                    }
                }
            }
        }
        else if (strcmp(cmd, CMD_LIST_USERS) == 0)
        {
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
            send(c->fd, out, strlen(out), 0);
        }
        else if (strcmp(cmd, CMD_CHALLENGE) == 0)
        {
            if (args)
            {
                char *from = strtok(args, " ");
                char *to = strtok(NULL, " ");
                if (from && to)
                {
                    if (strcmp(from, to) == 0)
                    {
                        send(c->fd, "ERROR cannot challenge yourself\n", strlen("ERROR cannot challenge yourself\n"), 0);
                    }
                    else
                    {
                        int rv = server_create_challenge(state, from, to);
                        if (rv == 0)
                        {
                            int notified = 0;
                            for (int j = 0; j < MAX_CLIENTS; ++j)
                            {
                                if (clients[j].fd != -1 && strcmp(clients[j].name, to) == 0)
                                {
                                    char out[PROTO_MAX_LINE];
                                    snprintf(out, sizeof(out), "CHALLENGE RECEIVED FROM %s\n", from);
                                    send(clients[j].fd, out, strlen(out), 0);
                                    notified = 1;
                                    break;
                                }
                            }
                            send(c->fd, "CHALLENGE_SENT\n", strlen("CHALLENGE_SENT\n"), 0);
                            if (!notified)
                            {
                                char resp[PROTO_MAX_LINE];
                                snprintf(resp, sizeof(resp), "WARNING target %s not online\n", to);
                                send(c->fd, resp, strlen(resp), 0);
                            }
                        }
                        else
                        {
                            send(c->fd, "ERROR no challenge slots available\n", strlen("ERROR no challenge slots available\n"), 0);
                        }
                    }
                }
            }
        }
        else if (strcmp(cmd, CMD_ACCEPT) == 0)
        {
            if (args)
            {
                char *acceptor = strtok(args, " ");
                char *challenger = strtok(NULL, " ");
                if (acceptor && challenger)
                {
                    uint64_t gid = 0;
                    int rv = server_accept_challenge(state, challenger, acceptor, &gid);
                    if (rv == 0)
                    {
                        int notified = 0;
                        for (int j = 0; j < MAX_CLIENTS; ++j)
                        {
                            if (clients[j].fd != -1 && strcmp(clients[j].name, challenger) == 0)
                            {
                                char out[PROTO_MAX_LINE];
                                snprintf(out, sizeof(out), "ACCEPTED %s %s %lu\n", acceptor, challenger, gid);
                                send(clients[j].fd, out, strlen(out), 0);
                                notified = 1;
                                break;
                            }
                        }
                        char out2[PROTO_MAX_LINE];
                        snprintf(out2, sizeof(out2), "ACCEPT_SENT %s %s %lu\n", acceptor, challenger, gid);
                        send(c->fd, out2, strlen(out2), 0);
                        if (!notified)
                        {
                            char resp[PROTO_MAX_LINE];
                            snprintf(resp, sizeof(resp), "WARNING challenger %s not online\n", challenger);
                            send(c->fd, resp, strlen(resp), 0);
                        }
                    }
                    else
                    {
                        char resp[PROTO_MAX_LINE];
                        snprintf(resp, sizeof(resp), "ERROR No active challenge from %s to %s\n", challenger, acceptor);
                        send(c->fd, resp, strlen(resp), 0);
                    }
                }
            }
        }
        else if (strcmp(cmd, CMD_REFUSE) == 0)
        {
            if (args)
            {
                char *refuser = strtok(args, " ");
                char *challenger = strtok(NULL, " ");
                if (refuser && challenger)
                {
                    int rv = server_refuse_challenge(state, challenger, refuser);
                    if (rv == 0)
                    {
                        int notified = 0;
                        for (int j = 0; j < MAX_CLIENTS; ++j)
                        {
                            if (clients[j].fd != -1 && strcmp(clients[j].name, challenger) == 0)
                            {
                                char out[PROTO_MAX_LINE];
                                snprintf(out, sizeof(out), "REFUSED %s %s\n", refuser, challenger);
                                send(clients[j].fd, out, strlen(out), 0);
                                notified = 1;
                                break;
                            }
                        }
                        char out2[PROTO_MAX_LINE];
                        snprintf(out2, sizeof(out2), "REFUSE_SENT %s %s\n", refuser, challenger);
                        send(c->fd, out2, strlen(out2), 0);
                        if (!notified)
                        {
                            char resp[PROTO_MAX_LINE];
                            snprintf(resp, sizeof(resp), "WARNING challenger %s not online\n", challenger);
                            send(c->fd, resp, strlen(resp), 0);
                        }
                    }
                    else
                    {
                        char resp[PROTO_MAX_LINE];
                        snprintf(resp, sizeof(resp), "ERROR No active challenge from %s to %s\n", challenger, refuser);
                        send(c->fd, resp, strlen(resp), 0);
                    }
                }
            }
        }
        else
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "ECHO %s\n", line);
            send(c->fd, resp, strlen(resp), 0);
        }

        line = nl + 1;
    }

    /* move leftover partial data to buffer head */
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

    client_t clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; ++i)
        client_init(&clients[i]);

    server_state_t state;
    server_init(&state);

    fd_set readset;
    while (1)
    {
        FD_ZERO(&readset);
        FD_SET(listen_fd, &readset);
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

        int rv = select(maxfd + 1, &readset, NULL, NULL, NULL);
        if (rv < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (FD_ISSET(listen_fd, &readset))
            server_accept_new(listen_fd, clients, MAX_CLIENTS);

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &readset))
                server_handle_client_data(&clients[i], clients, &state);
        }
    }

    close(listen_fd);
    return 0;
}

int main(int argc, char **argv)
{
    int port = ADDRESS_PORT;
    if (argc >= 2)
        port = atoi(argv[1]);
    return server_run(port);
}
