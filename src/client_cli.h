#ifndef CLIENT_CLI_H
#define CLIENT_CLI_H

#include <stdbool.h>
#include "protocol.h"

void client_cli_print_help(void);
void client_cli_print_prompt(const char *username, unsigned long focused_game_id);

// Parse a CLI line. If it represents a server command, write the protocol
// message into out and return true. Returns false for local commands.
bool client_cli_handle_input(const char *username, const char *line_in, char *out, int *sockfd);

#endif // CLIENT_CLI_H
