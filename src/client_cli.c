#define _POSIX_C_SOURCE 200112L
#include "client_cli.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "client.h"
#include "protocol.h"

void client_cli_print_help(void)
{
    printf("Commands:\n");
    printf("  connect <host> <port> - connect to server\n");
    printf("  register <name>       - set your username\n");
    printf("  bio <text>            - set your bio (or run 'bio' then enter up to 10 lines, end with a single . on its own line)\n");
    printf("  show_bio <username>   - show a user's bio, if empty show your own\n");
    printf("  list                  - request list of users\n");
    printf("  list_games            - request list of all ongoing games\n");
    printf("  challenge <user>      - challenge a user\n");
    printf("  accept <user>         - accept a challenge\n");
    printf("  refuse <user>         - refuse a challenge\n");
    printf("  chat <user|group> <message>\n");
    printf("  group_create <name>   - create a group chat you own\n");
    printf("  group_invite <name> <user> - invite user to your group\n");
    printf("  group_quit <name>     - quit a group chat\n");
    printf("  friend_add <user>     - add a friend\n");
    printf("  friend_remove <user>  - remove a friend\n");
    printf("  friends               - list your friends\n");
    printf("  focus <game-id>       - set active game\n");
    printf("  show_games            - list your ongoing games\n");
    printf("  set_private [game-id] <0|1>     - set privacy for the focused game\n");
    printf("  observe <game-id>       - start spectating a game\n");
    printf("  stop_observe           - stop spectating any game\n");
    printf("  move <pit|game pit>   - play a move in focused game or specific game\n");
    printf("  show_board [game-id]  - show board for focused game or specific id\n");
    printf("  get_replay <game-id>  - view replay of a finished game\n");
    printf("  help                  - show this help\n");
    printf("  quit                  - quit client\n\n\n");
}

void client_cli_print_prompt(const char *username, unsigned long focused_game_id)
{
    const char *GREEN = "\x1b[32m";
    const char *CYAN = "\x1b[36m";
    const char *RESET = "\x1b[0m";
    const char *YELLOW = "\x1b[33m";
    if (username && username[0] != '\0')
        if (focused_game_id != 0)
            printf("%sawalé%s %s%s%s %s[g-%lu]%s> ", CYAN, RESET, GREEN, username, RESET, YELLOW, focused_game_id, RESET);
        else
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
    if (!cmd)
        return false;
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
        // not allow name with spaces
        if (strchr(name, ' '))
        {
            printf("Username cannot contain spaces.\n");
            return false;
        }
        if (username && username[0] != '\0')
        {
            printf("You are already registered as '%s'. Disconnect or restart to change username.\n", username);
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
        char assembled[CLIENT_MAX_BIO];
        assembled[0] = '\0';
        if (!bio_text)
        {
            // Enter interactive multi-line bio mode
            printf("Enter your bio. Up to 10 lines. End with a single '.' on its own line.\n");
            char linebuf[256];
            int lines = 0;
            while (lines < 10)
            {
                if (!fgets(linebuf, sizeof(linebuf), stdin))
                    break;
                // remove trailing newline
                size_t L = strlen(linebuf);
                if (L && linebuf[L - 1] == '\n')
                    linebuf[--L] = '\0';
                if (strcmp(linebuf, ".") == 0)
                    break;
                if (assembled[0] != '\0')
                    strncat(assembled, "\\n", sizeof(assembled) - strlen(assembled) - 1);
                strncat(assembled, linebuf, sizeof(assembled) - strlen(assembled) - 1);
                lines++;
            }
        }
        else
        {
            // single-line bio provided; use as-is
            strncpy(assembled, bio_text, sizeof(assembled) - 1);
            assembled[sizeof(assembled) - 1] = '\0';
        }
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        // assembled contains literal newlines encoded as "\\n" sequences
        proto_build_bio_set(out, PROTO_MAX_LINE, username, assembled);
        return true;
    }
    else if (strcmp(cmd, "show_bio") == 0)
    {
        char *target_username = strtok(NULL, "");
        if (!target_username)
            target_username = (char *)username;
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
    else if (strcmp(cmd, "list_games") == 0)
    {
        proto_build_list_games(out, PROTO_MAX_LINE);
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
    else if (strcmp(cmd, "chat") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *target = strtok(NULL, " ");
        char *msg = strtok(NULL, "");
        if (!msg)
        {
            printf("Usage: chat <user|group> <message>\n");
            return false;
        }
        proto_build_chat(out, PROTO_MAX_LINE, username, target ? target : "", msg);
        return true;
    }
    else if (strcmp(cmd, "group_create") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *gname = strtok(NULL, " ");
        if (!gname)
        {
            printf("Usage: group_create <name>\n");
            return false;
        }
        proto_build_group_create(out, PROTO_MAX_LINE, username, gname);
        return true;
    }
    else if (strcmp(cmd, "group_invite") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *gname = strtok(NULL, " ");
        char *user = strtok(NULL, " \n");
        if (!gname || !user)
        {
            printf("Usage: group_invite <name> <user>\n");
            return false;
        }
        proto_build_group_invite(out, PROTO_MAX_LINE, username, gname, user);
        return true;
    }
    else if (strcmp(cmd, "group_quit") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *gname = strtok(NULL, " \n");
        if (!gname)
        {
            printf("Usage: group_quit <name>\n");
            return false;
        }
        proto_build_group_quit(out, PROTO_MAX_LINE, gname, username);
        return true;
    }
    else if (strcmp(cmd, "friend_add") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *user = strtok(NULL, " \n");
        if (!user)
        {
            printf("Usage: friend_add <user>\n");
            return false;
        }
        if (strcmp(user, username) == 0)
        {
            printf("Cannot add yourself as friend.\n");
            return false;
        }
        proto_build_friend_add(out, PROTO_MAX_LINE, username, user);
        return true;
    }
    else if (strcmp(cmd, "friend_remove") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *user = strtok(NULL, " \n");
        if (!user)
        {
            printf("Usage: friend_remove <user>\n");
            return false;
        }
        proto_build_friend_remove(out, PROTO_MAX_LINE, username, user);
        return true;
    }
    else if (strcmp(cmd, "friends") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        proto_build_list_friends(out, PROTO_MAX_LINE, username);
        return true;
    }
    else if (strcmp(cmd, "focus") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *gid = strtok(NULL, " \n");
        if (!gid)
        {
            printf("Usage: focus <game-id>\n");
            return false;
        }
        proto_build_focus(out, PROTO_MAX_LINE, username, strtoull(gid, NULL, 10));
        return true;
    }
    else if (strcmp(cmd, "show_games") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        proto_build_show_games(out, PROTO_MAX_LINE, username);
        return true;
    }
    else if (strcmp(cmd, "set_private") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *a = strtok(NULL, " \n");
        char *b = strtok(NULL, " \n");
        uint64_t gid = 0;
        int flag = -1;
        if (b)
        {
            gid = strtoull(a, NULL, 10);
            flag = atoi(b);
        }
        else if (a)
        {
            flag = atoi(a);
        }
        else
        {
            printf("Usage: set_private <0|1> or set_private <game-id> <0|1>\n");
            return false;
        }
        proto_build_set_private(out, PROTO_MAX_LINE, username, gid, flag);
        return true;
    }
    else if (strcmp(cmd, "observe") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *gid = strtok(NULL, " \n");
        if (!gid)
        {
            printf("Usage: observe <game-id>\n");
            return false;
        }
        proto_build_observe(out, PROTO_MAX_LINE, username, strtoull(gid, NULL, 10));
        return true;
    }
    else if (strcmp(cmd, "stop_observe") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        proto_build_stop_observe(out, PROTO_MAX_LINE, username, 0);
        return true;
    }
    else if (strcmp(cmd, "move") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *a = strtok(NULL, " \n");
        char *b = strtok(NULL, " \n");
        uint64_t gid = 0;
        int pit = -1;
        if (a && b)
        {
            gid = strtoull(a, NULL, 10);
            pit = atoi(b);
        }
        else if (a)
        {
            pit = atoi(a);
        }
        else
        {
            printf("Usage: move <pit|game-id pit>\n");
            return false;
        }
        proto_build_move(out, PROTO_MAX_LINE, username, gid, pit);
        return true;
    }
    else if (strcmp(cmd, "show_board") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *gid = strtok(NULL, " \n");
        proto_build_show_board(out, PROTO_MAX_LINE, username, gid ? strtoull(gid, NULL, 10) : 0);
        return true;
    }
    else if (strcmp(cmd, "get_replay") == 0)
    {
        if (username[0] == '\0')
        {
            printf("You must register a username first.\n");
            return false;
        }
        char *gid = strtok(NULL, " \n");
        if (!gid)
        {
            printf("Usage: get_replay <game-id>\n");
            return false;
        }
        proto_build_get_replay(out, PROTO_MAX_LINE, username, strtoull(gid, NULL, 10));
        return true;
    }
    else if (strcmp(cmd, "next") == 0)
    {
        snprintf(out, PROTO_MAX_LINE, "NEXT\n");
        return true;
    }
    else if (strcmp(cmd, "previous") == 0)
    {
        snprintf(out, PROTO_MAX_LINE, "PREVIOUS\n");
        return true;
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "EXIT") == 0)
    {
        snprintf(out, PROTO_MAX_LINE, "EXIT\n");
        return true;
    }

    printf("Unknown command '%s'\n", cmd);
    return false;
}
