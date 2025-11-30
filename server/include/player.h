#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"

// Player account management

int register_player(ServerState *state, const char *pseudo, const char *password);
int login_player(ServerState *state, const char *pseudo, const char *password);
PlayerAccount* find_player_by_pseudo(ServerState *state, const char *pseudo);

// Load/save accounts

int load_accounts(ServerState *state);
int save_accounts(ServerState *state);

#endif // PLAYER_H
