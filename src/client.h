#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include "game.h"

// Maximums for client-side buffers
#define CLIENT_MAX_MSG 1024
#define CLIENT_MAX_BIO 10 * 80 + 10
#define CLIENT_MAX_FRIENDS 32

// Representation of a connected/registered user profile kept by the
// server and shared with clients when needed. This struct only contains
// metadata; networking behavior is implemented elsewhere.
typedef struct
{
	char username[GAME_MAX_USERNAME];
	int socket_fd; // -1 if not connected (transient)

	// Short text bio provided by the user (up to 10 ASCII lines). Stored as
	// a single NUL-terminated string with newlines; server enforces limit.
	char bio[CLIENT_MAX_BIO]; // roughly 10 lines of 80 chars

	// Friends list: usernames allowed in private spectator lists and chat
	// filtering. Fixed-size simple list for now.
	char friends[CLIENT_MAX_FRIENDS][GAME_MAX_USERNAME];
	int friends_count;

	// Client-side state: list of game ids the user participates in.
	uint64_t active_games[16];
	int active_games_count;
} user_profile_t;

// Client-side message structure for chat and protocol exchange
typedef struct
{
	char from[GAME_MAX_USERNAME];
	char to[GAME_MAX_USERNAME]; // empty for global
	char text[CLIENT_MAX_MSG];
} client_message_t;

// Minimal client API prototypes (implementations elsewhere)
int client_send_register(int sockfd, const char *username);
int client_send_challenge(int sockfd, const char *from, const char *to);

#endif // CLIENT_H
