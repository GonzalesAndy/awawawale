#ifndef PROTOCOL_H
#define PROTOCOL_H

// Simple text protocol commands
#define CMD_LIST_USERS "LIST_USERS"
#define CMD_CHALLENGE "CHALLENGE"
#define CMD_ACCEPT "ACCEPT"
#define CMD_REFUSE "REFUSE"
#define CMD_MOVE "MOVE"
#define CMD_CHAT "CHAT"
#define CMD_REGISTER "REGISTER"
#define CMD_USERS "USERS"

#include <stddef.h>

// Max sizes for buffers
#define PROTO_MAX_LINE 512

// Build a protocol line into dest (null-terminated). Returns dest or NULL on error.
char *proto_build_list_users(char *dest, size_t n);
char *proto_build_challenge(char *dest, size_t n, const char *from, const char *to);
char *proto_build_accept(char *dest, size_t n, const char *from, const char *to);
char *proto_build_refuse(char *dest, size_t n, const char *from, const char *to);
char *proto_build_move(char *dest, size_t n, const char *from, int pit_index);
char *proto_build_chat(char *dest, size_t n, const char *from, const char *msg);

char *proto_build_register(char *dest, size_t n, const char *name);

// Parse a received line into command and args (in-place). Returns command pointer or NULL.
// The function will modify the line and return pointers into it for command and arg.
char *proto_parse_command(char *line, char **args);

#endif // PROTOCOL_H
