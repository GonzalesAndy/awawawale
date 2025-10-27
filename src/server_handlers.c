#include "server_handlers.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static void safe_send(int fd, const char *s)
{
    if (fd < 0 || !s) return;
    send(fd, s, strlen(s), 0);
}

// Very light parser: splits command and provides args pointer
static const char *parse_cmd(char *line, char **args)
{
    size_t L = strlen(line);
    if (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
    char *sp = strchr(line, ' ');
    if (sp) { *sp = '\0'; *args = sp + 1; }
    else { *args = NULL; }
    return line;
}

// Individual handlers forward to server_state API
static int handle_register(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args) return -1;
    char uname[GAME_MAX_USERNAME];
    strncpy(uname, args, sizeof(uname)-1); uname[sizeof(uname)-1] = '\0';
    if (self->name[0] != '\0' && strcmp(self->name, uname) != 0)
        server_unregister_user(state, self->name);
    if (server_register_user(state, uname, self->fd) == 0)
    {
        strncpy(self->name, uname, sizeof(self->name)-1); self->name[sizeof(self->name)-1] = '\0';
        char resp[PROTO_MAX_LINE];
        snprintf(resp, sizeof(resp), "REGISTERED %s\n", self->name);
        safe_send(self->fd, resp);
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR username %s already in use\n", uname);
    safe_send(self->fd, resp);
    return -1;
}

static int handle_bio(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self; (void)clients;
    if (!args) return -1;
    char *username = strtok(args, " ");
    char *bio_text = strtok(NULL, "");
    if (username && bio_text)
    {
        if (server_set_user_bio(state, username, bio_text) == 0)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "BIO_SET %s\n", username);
            safe_send(self->fd, resp);
            return 0;
        }
        char resp[PROTO_MAX_LINE];
        snprintf(resp, sizeof(resp), "ERROR user %s not found\n", username);
        safe_send(self->fd, resp);
    }
    return -1;
}

static int handle_bio_show(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)clients;
    if (!args) return -1;
    char *requester = strtok(args, " ");
    char *target = requester ? strtok(NULL, " \n") : NULL;
    if (!target) return -1;
    char biobuf[CLIENT_MAX_BIO];
    if (server_show_user_bio(state, requester, target, biobuf, sizeof(biobuf)) == 0)
    {
        char resp[PROTO_MAX_LINE];
        snprintf(resp, sizeof(resp), "BIO_SHOWN %s %s\n", target, biobuf);
        safe_send(self->fd, resp);
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR user %s not found\n", target);
    safe_send(self->fd, resp);
    return -1;
}

static int handle_list_users(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)args; (void)clients;
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
    safe_send(self->fd, out);
    return 0;
}

static int handle_challenge(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self;
    if (!args) return -1;
    char *from = strtok(args, " ");
    char *to = strtok(NULL, " ");
    if (!from || !to) return -1;
    if (strcmp(from, to) == 0)
    {
        safe_send(self->fd, "ERROR cannot challenge yourself\n");
        return -1;
    }
    int rv = server_create_challenge(state, from, to);
    if (rv == 0)
    {
        int notified = 0;
        for (int j = 0; j < SERVER_NET_MAX_CLIENTS; ++j)
        {
            if (clients[j].fd != -1 && strcmp(clients[j].name, to) == 0)
            {
                char out[PROTO_MAX_LINE];
                snprintf(out, sizeof(out), "CHALLENGE RECEIVED FROM %s\n", from);
                safe_send(clients[j].fd, out);
                notified = 1;
                break;
            }
        }
        safe_send(self->fd, "CHALLENGE_SENT\n");
        if (!notified)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "WARNING target %s not online\n", to);
            safe_send(self->fd, resp);
        }
        return 0;
    }
    safe_send(self->fd, "ERROR no challenge slots available\n");
    return -1;
}

static int handle_accept(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self;
    if (!args) return -1;
    char *acceptor = strtok(args, " ");
    char *challenger = strtok(NULL, " ");
    if (!acceptor || !challenger) return -1;
    uint64_t gid = 0;
    int rv = server_accept_challenge(state, challenger, acceptor, &gid);
    if (rv == 0)
    {
        int notified = 0;
        for (int j = 0; j < SERVER_NET_MAX_CLIENTS; ++j)
        {
            if (clients[j].fd != -1 && strcmp(clients[j].name, challenger) == 0)
            {
                char out[PROTO_MAX_LINE];
                snprintf(out, sizeof(out), "ACCEPTED %s %s %lu\n", acceptor, challenger, gid);
                safe_send(clients[j].fd, out);
                notified = 1;
                break;
            }
        }
        char out2[PROTO_MAX_LINE];
        snprintf(out2, sizeof(out2), "ACCEPT_SENT %s %s %lu\n", acceptor, challenger, gid);
        safe_send(self->fd, out2);
        if (!notified)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "WARNING challenger %s not online\n", challenger);
            safe_send(self->fd, resp);
        }
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR No active challenge from %s to %s\n", challenger, acceptor);
    safe_send(self->fd, resp);
    return -1;
}

static int handle_refuse(server_state_t *state, client_t *self, client_t *clients, char *args)
{
    (void)self;
    if (!args) return -1;
    char *refuser = strtok(args, " ");
    char *challenger = strtok(NULL, " ");
    if (!refuser || !challenger) return -1;
    int rv = server_refuse_challenge(state, challenger, refuser);
    if (rv == 0)
    {
        int notified = 0;
        for (int j = 0; j < SERVER_NET_MAX_CLIENTS; ++j)
        {
            if (clients[j].fd != -1 && strcmp(clients[j].name, challenger) == 0)
            {
                char out[PROTO_MAX_LINE];
                snprintf(out, sizeof(out), "REFUSED %s %s\n", refuser, challenger);
                safe_send(clients[j].fd, out);
                notified = 1;
                break;
            }
        }
        char out2[PROTO_MAX_LINE];
        snprintf(out2, sizeof(out2), "REFUSE_SENT %s %s\n", refuser, challenger);
        safe_send(self->fd, out2);
        if (!notified)
        {
            char resp[PROTO_MAX_LINE];
            snprintf(resp, sizeof(resp), "WARNING challenger %s not online\n", challenger);
            safe_send(self->fd, resp);
        }
        return 0;
    }
    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ERROR No active challenge from %s to %s\n", challenger, refuser);
    safe_send(self->fd, resp);
    return -1;
}

// Dispatcher
int server_dispatch_command(server_state_t *state, client_t *self, client_t *clients, const char *line_in)
{
    if (!state || !self || !line_in) return -1;
    char line[PROTO_MAX_LINE];
    strncpy(line, line_in, sizeof(line)-1); line[sizeof(line)-1] = '\0';
    char *args = NULL;
    const char *cmd = parse_cmd(line, &args);

    if (strcmp(cmd, CMD_REGISTER) == 0) return handle_register(state, self, clients, args);
    if (strcmp(cmd, CMD_BIO) == 0) return handle_bio(state, self, clients, args);
    if (strcmp(cmd, CMD_BIO_SHOW) == 0) return handle_bio_show(state, self, clients, args);
    if (strcmp(cmd, CMD_LIST_USERS) == 0) return handle_list_users(state, self, clients, args);
    if (strcmp(cmd, CMD_CHALLENGE) == 0) return handle_challenge(state, self, clients, args);
    if (strcmp(cmd, CMD_ACCEPT) == 0) return handle_accept(state, self, clients, args);
    if (strcmp(cmd, CMD_REFUSE) == 0) return handle_refuse(state, self, clients, args);

    char resp[PROTO_MAX_LINE];
    snprintf(resp, sizeof(resp), "ECHO %s\n", line_in);
    safe_send(self->fd, resp);
    return 0;
}
