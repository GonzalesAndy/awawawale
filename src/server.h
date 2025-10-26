#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "game.h"
#include "client.h"

// Maximums for the server in-memory tables
#define SERVER_MAX_USERS 128
#define SERVER_MAX_GAMES 64
#define SERVER_MAX_PENDING_CHALLENGES 64

// Challenge record for pending invitations
typedef struct {
	char from[GAME_MAX_USERNAME];
	char to[GAME_MAX_USERNAME];
	bool active;
} challenge_t;

// Server's main runtime state (in-memory). The server will maintain
// registries of users, active games, challenges, and chat logs. This is
// the central structure modified by server implementation code.
typedef struct {
	user_profile_t users[SERVER_MAX_USERS];
	int users_count;

	game_t games[SERVER_MAX_GAMES];
	int games_count;

	challenge_t challenges[SERVER_MAX_PENDING_CHALLENGES];
	int challenges_count;

	// Simple next ids generator for games
	uint64_t next_game_id;
} server_state_t;

// Server API prototypes (implementations elsewhere)
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

// Chat and messaging
int server_broadcast_message(server_state_t *s, const client_message_t *m);
int server_send_private_message(server_state_t *s, const client_message_t *m);

// Persistence
int server_persist_game_record(server_state_t *s, const game_t *g, const char *path);

#endif // SERVER_H

