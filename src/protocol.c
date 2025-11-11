#include "protocol.h"
#include <stdio.h>
#include <string.h>

char *proto_build_list_users(char *dest, size_t n) {
    if (!dest || n < 1) return NULL;
    snprintf(dest, n, "%s\n", CMD_LIST_USERS);
    return dest;
}

char *proto_build_challenge(char *dest, size_t n, const char *from, const char *to) {
    if (!dest || !from || !to) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_CHALLENGE, from, to);
    return dest;
}

char *proto_build_accept(char *dest, size_t n, const char *from, const char *to) {
    if (!dest || !from || !to) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_ACCEPT, from, to);
    return dest;
}

char *proto_build_refuse(char *dest, size_t n, const char *from, const char *to) {
    if (!dest || !from || !to) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_REFUSE, from, to);
    return dest;
}

char *proto_build_move(char *dest, size_t n, const char *from, uint64_t game_id, int pit_index){
    if (!dest || !from) return NULL;
    snprintf(dest, n, "%s %s %lu %d\n", CMD_MOVE, from, game_id, pit_index);
    return dest;
}

char *proto_build_chat(char *dest, size_t n, const char *from, const char *to, const char *msg) {
    if (!dest || !from || !msg) return NULL;
    if (to && to[0] != '\0') {
        snprintf(dest, n, "%s %s %s %s\n", CMD_CHAT, from, to, msg);
    } else {
        snprintf(dest, n, "%s %s %s\n", CMD_CHAT, from, msg);
    }
    return dest;
}

char *proto_build_focus(char *dest, size_t n, const char *from, uint64_t game_id) {
    if (!dest || !from) return NULL;
    snprintf(dest, n, "%s %s %lu\n", CMD_FOCUS, from, game_id);
    return dest;
}

char *proto_build_show_games(char *dest, size_t n, const char *from) {
    if (!dest || !from) return NULL;
    snprintf(dest, n, "%s %s\n", CMD_SHOW_GAMES, from);
    return dest;
}

char *proto_build_list_games(char *dest, size_t n) {
    if (!dest) return NULL;
    snprintf(dest, n, "%s\n", CMD_LIST_GAMES);
    return dest;
}

char *proto_build_show_board(char *dest, size_t n, const char *from, uint64_t game_id) {
    if (!dest || !from) return NULL;
    snprintf(dest, n, "%s %s %lu\n", CMD_SHOW_BOARD, from, game_id);
    return dest;
}

char *proto_build_group_create(char *dest, size_t n, const char *owner, const char *group_name) {
    if (!dest || !owner || !group_name) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_GROUP_CREATE, owner, group_name);
    return dest;
}

char *proto_build_group_invite(char *dest, size_t n, const char *owner, const char *group_name, const char *username) {
    if (!dest || !owner || !group_name || !username) return NULL;
    snprintf(dest, n, "%s %s %s %s\n", CMD_GROUP_INVITE, owner, group_name, username);
    return dest;
}

char *proto_build_group_quit(char *dest, size_t n, const char *group_name, const char *username) {
    if (!dest || !group_name || !username) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_GROUP_QUIT, group_name, username);
    return dest;
}

char *proto_build_register(char *dest, size_t n, const char *name) {
    if (!dest || !name) return NULL;
    snprintf(dest, n, "%s %s\n", CMD_REGISTER, name);
    return dest;
}

char *proto_build_bio_set(char *dest, size_t n, const char *from, const char *bio_text) {
    if (!dest || !from || !bio_text) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_BIO, from, bio_text);
    return dest;
}

char *proto_build_bio_show(char *dest, size_t n, const char *requester, const char *target_username) {
    if (!dest || !requester || !target_username) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_BIO_SHOW, requester, target_username);
    return dest;
}

char *proto_build_friend_add(char *dest, size_t n, const char *owner, const char *friend_username) {
    if (!dest || !owner || !friend_username) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_FRIEND_ADD, owner, friend_username);
    return dest;
}

char *proto_build_friend_remove(char *dest, size_t n, const char *owner, const char *friend_username) {
    if (!dest || !owner || !friend_username) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_FRIEND_REMOVE, owner, friend_username);
    return dest;
}

char *proto_build_list_friends(char *dest, size_t n, const char *owner) {
    if (!dest || !owner) return NULL;
    snprintf(dest, n, "%s %s\n", CMD_LIST_FRIENDS, owner);
    return dest;
}

char *proto_build_set_private(char *dest, size_t n, const char *from, uint64_t game_id, int private_flag) {
    if (!dest || !from) return NULL;
    snprintf(dest, n, "%s %s %lu %d\n", CMD_SET_PRIVATE, from, (unsigned long)game_id, private_flag ? 1 : 0);
    return dest;
}

char *proto_parse_command(char *line, char **args) {
    if (!line) return NULL;
    size_t L = strlen(line);
    if (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
    char *sp = strchr(line, ' ');
    if (sp) {
        *sp = '\0';
        *args = sp + 1;
    } else {
        *args = NULL;
    }
    return line;
}
