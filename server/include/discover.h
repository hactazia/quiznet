#ifndef DISCOVER_H
#define DISCOVER_H

#include "server.h"

#ifdef _WIN32
#include <winsock2.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

void* udp_discovery_handler(void *arg);
void send_discovery_response(ServerState *state, struct sockaddr_in *client_addr, socklen_t addr_len);

#endif // DISCOVER_H
