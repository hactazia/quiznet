#ifndef HANDLERS_PLAYER_H
#define HANDLERS_PLAYER_H

#include "types.h"
#include "cJSON.h"

// Player authentication handlers
void handle_register(ServerState *state, Client *client, cJSON *json);
void handle_login(ServerState *state, Client *client, cJSON *json);

#endif // HANDLERS_PLAYER_H
