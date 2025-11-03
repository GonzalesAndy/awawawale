#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

#include "server_net.h"
#include "server.h"

// Dispatch a single parsed command line (null-terminated, without trailing \n)
// Returns 0 on success, negative on error
int server_dispatch_command(server_state_t *state, client_t *self, client_t *clients, const char *line);

#endif // SERVER_HANDLERS_H
