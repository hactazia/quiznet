#ifndef HANDLERS_COMMON_H
#define HANDLERS_COMMON_H

#include "types.h"
#include "cJSON.h"

// Send message to a specific client
int send_to_client(ServerState *state, int client_id, const char *message);

// Send message to all clients in a session
int broadcast_to_session(ServerState *state, Session *session, const char *message);

// Error responses
void send_error(Client *client, const char *action, const char *status, const char *message);
void send_bad_request(Client *client);
void send_unknown_error(Client *client);

#endif // HANDLERS_COMMON_H
