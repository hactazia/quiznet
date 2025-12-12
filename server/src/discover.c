#include "discover.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/**
 * Sends a discovery response to a client.
 * Formats server name and TCP port into response message.
 * @param state Server state for getting server info
 * @param client_addr Client address to respond to
 * @param addr_len Size of client address structure
 */
void send_discovery_response(ServerState* state,
                             struct sockaddr_in* client_addr,
                             socklen_t addr_len) {
  char response[256];
  snprintf(response, sizeof(response), "hello i'm a quiznet server:%s:%d",
           state->server_name, state->tcp_port);

  log_msg("DISCOVER", "Sending response: '%s'", response);
  sendto(state->udp_socket, response, strlen(response), 0,
         (struct sockaddr*)client_addr, addr_len);
}

/**
 * Thread handler for UDP server discovery requests.
 * Listens for "looking for quiznet servers" broadcast messages.
 * Responds with server name and TCP port for connection.
 * @param arg Pointer to ServerState
 * @return NULL when thread exits
 */
void* udp_discovery_handler(void* arg) {
  ServerState* state = (ServerState*)arg;
  log_msg("DISCOVER", "UDP discovery handler started on port %d\n",
         state->udp_port);

  char buffer[256];
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  while (state->running) {
    int received = recvfrom(state->udp_socket, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr*)&client_addr, &addr_len);

    if (received > 0) {
      buffer[received] = '\0';
      log_msg("DISCOVER", "Received %d bytes from %s:%d: '%s'", received,
             inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
             buffer);

      // Check for discovery request
      if (strcmp(buffer, "looking for quiznet servers") == 0) {
        log_msg("DISCOVER", "Discovery request received");
        send_discovery_response(state, &client_addr, addr_len);
      } else
        log_msg("DISCOVER", "Unknown message, ignoring");
    }
  }

  log_msg("DISCOVER", "UDP discovery handler stopped");
  return NULL;
}
