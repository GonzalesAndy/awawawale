#ifndef SERVER_NET_H
#define SERVER_NET_H

#include <stddef.h>
#include "server.h"
#include "protocol.h"

#define SERVER_NET_MAX_CLIENTS 128

// Per-connection state for the network layer
typedef struct {
    int fd;
    char name[GAME_MAX_USERNAME];
    char buf[PROTO_MAX_LINE];
    size_t buf_len;
    uint64_t focused_game_id; // 0 means none
} client_t;

// Start the server loop and block; returns 0 on clean shutdown, >0 on error
int server_run(int port);

#endif // SERVER_NET_H
