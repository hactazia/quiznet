#include "protocol.h"
#include "handlers/common.h"
#include "handlers/player.h"
#include "handlers/session.h"
#include "handlers/game.h"
#include "handlers/joker.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

/**
 * Main request router for incoming client messages.
 * Parses METHOD endpoint format, extracts JSON body, routes to handler.
 * @param state Server state for all operations
 * @param client Client making the request
 * @param request Raw request string ({method} {endpoint}\n{json})
 */
void handle_request(ServerState *state, Client *client, const char *request) {
    char method[16] = "";
    char endpoint[64] = "";
    char *json_start = NULL;
    
    if (sscanf(request, "%15s %63s", method, endpoint) < 2) {
        log_msg("PROTOCOL", "handle_request() FAILED - cannot parse request");
        send_bad_request(client);
        return;
    }
    
    json_start = strchr(request, '{');
    cJSON *json = NULL;
    if (json_start) {
        json = cJSON_Parse(json_start);
        if (!json) {
            log_msg("PROTOCOL", "handle_request() WARNING - failed to parse JSON");
        }
    }
    
    log_msg("PROTOCOL", "Request: %s %s (client %d)", method, endpoint, client->id);

    if (json) {
        char *json_pretty = cJSON_Print(json);
        free(json_pretty);
    }
    
    if (strcmp(method, "POST") == 0) {
        if (strcmp(endpoint, "player/register") == 0) {
            if (json) handle_register(state, client, json);
            else send_bad_request(client);
        }
        else if (strcmp(endpoint, "player/login") == 0) {
            if (json) handle_login(state, client, json);
            else send_bad_request(client);
        }
        else if (strcmp(endpoint, "session/create") == 0) {
            if (json) handle_create_session(state, client, json);
            else send_bad_request(client);
        }
        else if (strcmp(endpoint, "session/join") == 0) {
            if (json) handle_join_session(state, client, json);
            else send_bad_request(client);
        }
        else if (strcmp(endpoint, "session/start") == 0) {
            handle_start_session(state, client);
        }
        else if (strcmp(endpoint, "question/answer") == 0) {
            if (json) handle_answer(state, client, json);
            else send_bad_request(client);
        }
        else if (strcmp(endpoint, "joker/use") == 0) {
            if (json) handle_joker(state, client, json);
            else send_bad_request(client);
        }
        else {
            log_msg("PROTOCOL", "Unknown POST endpoint: %s", endpoint);
            send_unknown_error(client);
        }
    }
    else if (strcmp(method, "GET") == 0) {
        if (strcmp(endpoint, "themes/list") == 0) {
            handle_get_themes(state, client);
        }
        else if (strcmp(endpoint, "sessions/list") == 0) {
            handle_get_sessions(state, client);
        }
        else {
            log_msg("PROTOCOL", "Unknown GET endpoint: %s", endpoint);
            send_unknown_error(client);
        }
    }
    else {
        log_msg("PROTOCOL", "Unknown method: %s", method);
        send_bad_request(client);
    }
    
    if (json) {
        cJSON_Delete(json);
    }
}
