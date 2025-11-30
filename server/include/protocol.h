#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "types.h"
#include "cJSON.h"

#include "handlers/common.h"
#include "handlers/player.h"
#include "handlers/session.h"
#include "handlers/game.h"
#include "handlers/joker.h"

void handle_request(ServerState *state, Client *client, const char *request);

#endif // PROTOCOL_H
