#ifndef HANDLERS_SESSION_H
#define HANDLERS_SESSION_H

#include "types.h"
#include "cJSON.h"

// Session management handlers
void handle_get_sessions(ServerState *state, Client *client);
void handle_create_session(ServerState *state, Client *client, cJSON *json);
void handle_join_session(ServerState *state, Client *client, cJSON *json);
void handle_start_session(ServerState *state, Client *client);

#endif // HANDLERS_SESSION_H
