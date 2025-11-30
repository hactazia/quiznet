#ifndef HANDLERS_GAME_H
#define HANDLERS_GAME_H

#include "types.h"
#include "cJSON.h"

// Game flow handlers
void handle_get_themes(ServerState *state, Client *client);
void handle_answer(ServerState *state, Client *client, cJSON *json);

#endif // HANDLERS_GAME_H
