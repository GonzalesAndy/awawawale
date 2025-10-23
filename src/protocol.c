#include "protocol.h"
#include <stdio.h>
#include <string.h>

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
