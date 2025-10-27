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

/* Build a GAME_UPDATE line: "GAME_UPDATE <game_id> <board_text>\n" */
char *proto_build_game_update(char *dest, size_t n, uint64_t game_id, const char *board_text) {
    if (!dest || !board_text) return NULL;
    /* board_text is expected to be a single-line representation (no newlines)
     * We simply format: CMD_GAME_UPDATE <id> <board_text>\n */
    snprintf(dest, n, "%s %lu %s\n", CMD_GAME_UPDATE, (unsigned long)game_id, board_text);
    return dest;
}

/* Build a GAME_START_AT line: "GAME_START_AT <game_id> <start_ts>\n" */
char *proto_build_game_start_at(char *dest, size_t n, uint64_t game_id, uint64_t start_ts) {
    if (!dest) return NULL;
    snprintf(dest, n, "%s %lu %lu\n", CMD_GAME_START_AT, (unsigned long)game_id, (unsigned long)start_ts);
    return dest;
}

char *proto_build_game_mode(char *dest, size_t n, uint64_t game_id, const char *mode) {
    if (!dest || !mode) return NULL;
    snprintf(dest, n, "%s %lu %s\n", CMD_GAME_UPDATE[0]=='\0'?"GAME_MODE":CMD_GAME_UPDATE, (unsigned long)game_id, mode);
    /* Above line uses CMD_GAME_UPDATE fallback only to avoid including new macro; better to use literal */
    snprintf(dest, n, "%s %lu %s\n", "GAME_MODE", (unsigned long)game_id, mode);
    return dest;
}
