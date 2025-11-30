#include "handlers/game.h"
#include "handlers/common.h"
#include "session.h"
#include "question.h"
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
 * Handles request for available themes list.
 * Returns all loaded themes with IDs and names.
 * @param state Server state with themes list
 * @param client Client making the request
 */
void handle_get_themes(ServerState *state, Client *client) {
    log_msg("PROTOCOL", "handle_get_themes() - client %d, %d themes available", 
           client->id, state->num_themes);
    cJSON *response = create_themes_json(state);
    
    char *json_str = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(response);
}

/**
 * Handles answer submission from a player.
 * Supports QCM (index), text, and boolean answer types.
 * @param state Server state for session lookup
 * @param client Client submitting answer
 * @param json Request body with answer and responseTime
 */
void handle_answer(ServerState *state, Client *client, cJSON *json) {
    log_msg("PROTOCOL", "handle_answer() - client %d, session %d", 
           client->id, client->current_session_id);
    
    if (client->current_session_id < 0) {
        log_msg("PROTOCOL", "handle_answer() FAILED - not in a session");
        send_error(client, "question/answer", "400", "not in a session");
        return;
    }
    
    Session *session = find_session(state, client->current_session_id);
    if (!session || session->status != SESSION_PLAYING) {
        log_msg("PROTOCOL", "handle_answer() FAILED - session not playing");
        send_error(client, "question/answer", "400", "session not playing");
        return;
    }
    
    cJSON *answer = cJSON_GetObjectItem(json, "answer");
    cJSON *response_time = cJSON_GetObjectItem(json, "responseTime");
    
    if (!response_time) {
        log_msg("PROTOCOL", "handle_answer() FAILED - missing responseTime");
        send_bad_request(client);
        return;
    }
    
    int answer_index = -1;
    char text_answer[MAX_ANSWER_TEXT] = "";
    bool bool_answer = false;
    
    if (cJSON_IsNumber(answer)) {
        answer_index = answer->valueint;
        log_msg("PROTOCOL", "Answer: index=%d, responseTime=%.2f", 
               answer_index, response_time->valuedouble);
    } else if (cJSON_IsString(answer)) {
        strncpy(text_answer, answer->valuestring, MAX_ANSWER_TEXT - 1);
        log_msg("PROTOCOL", "Answer: text='%s', responseTime=%.2f", 
               text_answer, response_time->valuedouble);
    } else if (cJSON_IsBool(answer)) {
        bool_answer = cJSON_IsTrue(answer);
        log_msg("PROTOCOL", "Answer: bool=%s, responseTime=%.2f", 
               bool_answer ? "true" : "false", response_time->valuedouble);
    }
    
    process_answer(state, session, client->id, answer_index, text_answer, bool_answer, 
                  response_time->valuedouble);
    
    // Send acknowledgment
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "action", "question/answer");
    cJSON_AddStringToObject(resp, "statut", "200");
    cJSON_AddStringToObject(resp, "message", "answer received");
    
    char *json_str = cJSON_PrintUnformatted(resp);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(resp);
}
