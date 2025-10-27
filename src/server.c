// Keep a tiny entry point and delegate to server_net
#include <stdlib.h>
#include <stdio.h>
#include "server_net.h"

#define ADDRESS_PORT 9987

int main(int argc, char **argv)
{
    int port = ADDRESS_PORT;
    if (argc >= 2) port = atoi(argv[1]);
    return server_run(port);
}
