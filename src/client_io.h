#ifndef CLIENT_IO_H
#define CLIENT_IO_H

#include <stdbool.h>

// Connect to host:port, return socket fd or -1
int client_io_connect(const char *host, const char *port);

#endif // CLIENT_IO_H
