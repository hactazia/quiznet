#ifndef SERVER_H
#define SERVER_H

#include "types.h"

int init_server(ServerState *state, int tcp_port, int udp_port);
void cleanup_server(ServerState *state);
void run_server(ServerState *state);
void stop_server(ServerState *state);
Client* accept_client(ServerState *state);
void disconnect_client(ServerState *state, Client *client);
void* client_handler(void *arg);
void* udp_discovery_handler(void *arg);

#endif // SERVER_H
