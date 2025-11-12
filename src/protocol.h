
#ifndef PROTOCOL_H
#define PROTOCOL_H

// Protocol commands (text-based simple protocol)
#define CMD_LIST_USERS "LIST_USERS"
#define CMD_CHALLENGE "CHALLENGE"
#define CMD_ACCEPT "ACCEPT"
#define CMD_REFUSE "REFUSE"
#define CMD_MOVE "MOVE"
#define CMD_FOCUS "FOCUS"
#define CMD_SHOW_GAMES "SHOW_GAMES"
#define CMD_LIST_GAMES "LIST_GAMES"
#define CMD_SHOW_BOARD "SHOW_BOARD"
#define CMD_CHAT "CHAT"
#define CMD_GROUP_CREATE "GROUP_CREATE"
#define CMD_GROUP_INVITE "GROUP_INVITE"
#define CMD_GROUP_QUIT "GROUP_QUIT"
#define CMD_REGISTER "REGISTER"
#define CMD_USERS "USERS"
#define CMD_FRIENDS "FRIENDS"
#define CMD_GAME_UPDATE "GAME_UPDATE"
#define CMD_OBSERVE "OBSERVE"
#define CMD_STOP_OBSERVE "STOP_OBSERVE"
#define CMD_BIO "BIO"
#define CMD_BIO_SHOW "BIO_SHOW"
#define CMD_SET_PRIVATE "SET_PRIVATE"
#define CMD_ALLOW_SPECTATOR "ALLOW_SPECTATOR"
#define CMD_DISALLOW_SPECTATOR "DISALLOW_SPECTATOR"
#define CMD_GET_REPLAY "GET_REPLAY"
#define CMD_REPLAY_NEXT "NEXT"
#define CMD_REPLAY_PREV "PREVIOUS"
#define CMD_REPLAY_EXIT "EXIT"

// Friend management commands
#define CMD_FRIEND_ADD "FRIEND_ADD"
#define CMD_FRIEND_REMOVE "FRIEND_REMOVE"
#define CMD_LIST_FRIENDS "LIST_FRIENDS"

#include <stddef.h>
#include <stdint.h>

// Max sizes for buffers
#define PROTO_MAX_LINE 2048

// Build a protocol line into dest. Returns dest or NULL on error.
char *proto_build_list_users(char *dest, size_t n);
char *proto_build_challenge(char *dest, size_t n, const char *from, const char *to);
char *proto_build_accept(char *dest, size_t n, const char *from, const char *to);
char *proto_build_refuse(char *dest, size_t n, const char *from, const char *to);
char *proto_build_move(char *dest, size_t n, const char *from, uint64_t game_id, int pit_index);
char *proto_build_focus(char *dest, size_t n, const char *from, uint64_t game_id);
char *proto_build_show_games(char *dest, size_t n, const char *from);
char *proto_build_list_games(char *dest, size_t n);
char *proto_build_show_board(char *dest, size_t n, const char *from, uint64_t game_id);
char *proto_build_chat(char *dest, size_t n, const char *from, const char *to, const char *msg);
char *proto_build_group_create(char *dest, size_t n, const char *owner, const char *group_name);
char *proto_build_group_invite(char *dest, size_t n, const char *owner, const char *group_name, const char *username);
char *proto_build_group_quit(char *dest, size_t n, const char *group_name, const char *username);
char *proto_build_register(char *dest, size_t n, const char *name);
char *proto_build_game_update(char *dest, size_t n, uint64_t game_id, const char *board_text);
char *proto_build_observe(char *dest, size_t n, const char *from, uint64_t game_id);
char *proto_build_get_replay(char *dest, size_t n, const char *from, uint64_t game_id);
char *proto_build_stop_observe(char *dest, size_t n, const char *from, uint64_t game_id);
char *proto_build_bio_set(char *dest, size_t n, const char *from, const char *bio_text);
char *proto_build_bio_show(char *dest, size_t n, const char *requester, const char *target_username);
char *proto_build_set_private(char *dest, size_t n, const char *from, uint64_t game_id, int private_flag);
char *proto_build_allow_spectator(char *dest, size_t n, const char *from, const char *spectator_username);
char *proto_build_disallow_spectator(char *dest, size_t n, const char *from, const char *spectator_username);

// Friend management builders
char *proto_build_friend_add(char *dest, size_t n, const char *owner, const char *friend_username);
char *proto_build_friend_remove(char *dest, size_t n, const char *owner, const char *friend_username);
char *proto_build_list_friends(char *dest, size_t n, const char *owner);

// Parse a received line into command and args. Returns command pointer or NULL.
// The function will modify the line and return pointers into it for command and arg.
char *proto_parse_command(char *line, char **args);

#endif // PROTOCOL_H
