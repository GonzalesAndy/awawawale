#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include "game.h"

// Maximums client buffers
#define CLIENT_MAX_MSG 1024
#define CLIENT_MAX_BIO 10 * 80 + 10
#define CLIENT_MAX_FRIENDS 32

// Representation of a connected/registered user profile kept on the server
typedef struct
{
	char username[GAME_MAX_USERNAME];
	int socket_fd; // -1 if not connected

	char bio[CLIENT_MAX_BIO]; // roughly 10 lines of 80 chars

	// users allowed in private spectator mode
	char friends[CLIENT_MAX_FRIENDS][GAME_MAX_USERNAME];
	int friends_count;

	// list of game ids the user participates in.
	uint64_t active_games[16];
	int active_games_count;
} user_profile_t;

#endif // CLIENT_H
