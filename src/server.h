#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "game.h"
#include "client.h"
#include "persist.h"

// Maximums for the server in-memory tables
#define SERVER_MAX_USERS 128
#define SERVER_MAX_GAMES 64
#define SERVER_MAX_PENDING_CHALLENGES 64
// Groups
#define SERVER_MAX_GROUPS 64
#define SERVER_MAX_GROUP_MEMBERS 32

// pending game invitations
typedef struct {
	char from[GAME_MAX_USERNAME];
	char to[GAME_MAX_USERNAME];
	bool active;
} challenge_t;

// group chat instance
typedef struct {
	char name[GAME_MAX_USERNAME];
	char owner[GAME_MAX_USERNAME];
	char members[SERVER_MAX_GROUP_MEMBERS][GAME_MAX_USERNAME];
	int member_count;
	bool active;
} group_t;

// Server's main state structure
typedef struct {
	user_profile_t users[SERVER_MAX_USERS];
	int users_count;

	game_t games[SERVER_MAX_GAMES];
	int games_count;

	challenge_t challenges[SERVER_MAX_PENDING_CHALLENGES];
	int challenges_count;

	// Group chats
	group_t groups[SERVER_MAX_GROUPS];
	int groups_count;

	uint64_t next_game_id;
} server_state_t;

// Initialization and user management
int server_init(server_state_t *s);
int server_register_user(server_state_t *s, const char *username, int sockfd);
int server_unregister_user(server_state_t *s, const char *username);
int server_list_online_users(server_state_t *s, char dest[][GAME_MAX_USERNAME], int max);
int server_set_user_bio(server_state_t *s, const char *username, const char *bio_text);
int server_show_user_bio(server_state_t *s, const char *requester, const char *target_username, char *out_bio, size_t n);

// Challenge management
int server_create_challenge(server_state_t *s, const char *from, const char *to);
int server_cancel_challenge(server_state_t *s, const char *from, const char *to);
int server_accept_challenge(server_state_t *s, const char *from, const char *to, uint64_t *out_game_id);
int server_refuse_challenge(server_state_t *s, const char *from, const char *to);

// Game lifecycle management
int server_create_game_from_challenge(server_state_t *s, const char *player_a, const char *player_b, uint64_t *out_game_id);
int server_get_game(server_state_t *s, uint64_t game_id, game_t **out);
int server_remove_game(server_state_t *s, uint64_t game_id);

// Groups management
int server_group_create(server_state_t *s, const char *owner, const char *group_name);
int server_group_invite(server_state_t *s, const char *owner, const char *group_name, const char *username);
int server_group_quit(server_state_t *s, const char *group_name, const char *username);
int server_group_is_member(server_state_t *s, const char *group_name, const char *username);

// Friends management
int server_friend_add(server_state_t *s, const char *owner, const char *friend_username);
int server_friend_remove(server_state_t *s, const char *owner, const char *friend_username);
int server_friend_list(server_state_t *s, const char *owner, char dest[][GAME_MAX_USERNAME], int max);

// Connection state
int server_mark_user_offline(server_state_t *s, const char *username);

#endif // SERVER_H

