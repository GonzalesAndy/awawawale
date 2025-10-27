#define _POSIX_C_SOURCE 200112L
#include "client_cli.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "protocol.h"

void client_cli_print_help(void)
{
    printf("Commands:\n");
    printf("  connect <host> <port> - connect to server\n");
    printf("  register <name>       - set your username\n");
    printf("  bio <text>            - set your bio\n");
    printf("  show_bio <username>   - show a user's bio, if empty show your own\n");
    printf("  list                  - request list of users\n");
    printf("  challenge <user>      - challenge a user\n");
    printf("  accept <user>         - accept a challenge\n");
    printf("  refuse <user>         - refuse a challenge\n");
    printf("  help                  - show this help\n");
    printf("  quit                  - exit\n\n\n");
}

void client_cli_print_prompt(const char *username)
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

// networking is implemented in client_io; no direct connect helper here

bool client_cli_handle_input(const char *username, const char *line_in, char *out, int *sockfd)
{
    char tmp[PROTO_MAX_LINE];
    strncpy(tmp, line_in, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *cmd = strtok(tmp, " ");
    if (!cmd) return false;
    out[0] = '\0';

    if (strcmp(cmd, "help") == 0)
    {
        client_cli_print_help();
        return false;
    }
    else if (strcmp(cmd, "quit") == 0)
    {
        return false;
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
        // delegate to client_io via client.c main loop; here we only signal local
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
        if (!target_username) target_username = (char *)username;
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

    printf("Unknown command '%s'\n", cmd);
    return false;
}
