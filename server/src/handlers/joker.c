#include "handlers/joker.h"
#include "handlers/common.h"
#include "session.h"
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
 * Handles joker usage request.
 * Supports 'fifty' (50/50) and 'skip' joker types.
 * @param state Server state for session lookup
 * @param client Client using the joker
 * @param json Request body with joker type
 */
void handle_joker(ServerState *state, Client *client, cJSON *json) {
    log_msg("PROTOCOL", "handle_joker() - client %d, session %d", 
           client->id, client->current_session_id);
    
    if (client->current_session_id < 0) {
        log_msg("PROTOCOL", "handle_joker() FAILED - not in a session");
        send_error(client, "joker/use", "400", "not in a session");
        return;
    }
    
    Session *session = find_session(state, client->current_session_id);
    if (!session || session->status != SESSION_PLAYING) {
        log_msg("PROTOCOL", "handle_joker() FAILED - session not playing");
        send_error(client, "joker/use", "400", "session not playing");
        return;
    }
    
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (!type || !cJSON_IsString(type)) {
        log_msg("PROTOCOL", "handle_joker() FAILED - missing type");
        send_bad_request(client);
        return;
    }
    
    log_msg("PROTOCOL", "Joker type: '%s'", type->valuestring);
    
    SessionPlayer *player = find_session_player(session, client->id);
    if (!player) {
        log_msg("PROTOCOL", "handle_joker() FAILED - player not found in session");
        send_error(client, "joker/use", "400", "player not found");
        return;
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "joker/use");
    
    if (strcmp(type->valuestring, "fifty") == 0) {
        int removed[2];
        if (use_joker_fifty(state, session, client->id, removed) == 0) {
            cJSON_AddStringToObject(response, "statut", "200");
            cJSON_AddStringToObject(response, "message", "joker activated");
            
            // Get remaining answers
            Question *q = NULL;
            for (int i = 0; i < state->num_questions; i++) {
                if (state->questions[i].id == session->question_ids[session->current_question]) {
                    q = &state->questions[i];
                    break;
                }
            }
            
            if (q) {
                cJSON *remaining = cJSON_AddArrayToObject(response, "remainingAnswers");
                for (int i = 0; i < 4; i++) {
                    if (i != removed[0] && i != removed[1]) {
                        cJSON_AddItemToArray(remaining, cJSON_CreateString(q->answers[i]));
                    }
                }
            }
            
            cJSON *jokers = cJSON_AddObjectToObject(response, "jokers");
            cJSON_AddNumberToObject(jokers, "fifty", 0);
            cJSON_AddNumberToObject(jokers, "skip", player->joker_skip_used ? 0 : 1);
        } else {
            cJSON_AddStringToObject(response, "statut", "400");
            cJSON_AddStringToObject(response, "message", "joker not available");
        }
    } else if (strcmp(type->valuestring, "skip") == 0) {
        if (use_joker_skip(state, session, client->id) == 0) {
            cJSON_AddStringToObject(response, "statut", "200");
            cJSON_AddStringToObject(response, "message", "question skipped");
            
            cJSON *jokers = cJSON_AddObjectToObject(response, "jokers");
            cJSON_AddNumberToObject(jokers, "fifty", player->joker_fifty_used ? 0 : 1);
            cJSON_AddNumberToObject(jokers, "skip", 0);
        } else {
            cJSON_AddStringToObject(response, "statut", "400");
            cJSON_AddStringToObject(response, "message", "joker not available");
        }
    } else {
        cJSON_AddStringToObject(response, "statut", "400");
        cJSON_AddStringToObject(response, "message", "unknown joker type");
    }
    
    char *json_str = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(response);
}
