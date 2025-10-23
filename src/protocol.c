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

char *proto_build_move(char *dest, size_t n, const char *from, int pit_index) {
    if (!dest || !from) return NULL;
    snprintf(dest, n, "%s %s %d\n", CMD_MOVE, from, pit_index);
    return dest;
}

char *proto_build_chat(char *dest, size_t n, const char *from, const char *msg) {
    if (!dest || !from || !msg) return NULL;
    snprintf(dest, n, "%s %s %s\n", CMD_CHAT, from, msg);
    return dest;
}

char *proto_build_register(char *dest, size_t n, const char *name) {
    if (!dest || !name) return NULL;
    snprintf(dest, n, "%s %s\n", CMD_REGISTER, name);
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
