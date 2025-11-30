#include "handlers/session.h"
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
 * Handles request for available sessions list.
 * Returns all waiting sessions that can be joined.
 * @param state Server state with sessions list
 * @param client Client making the request
 */
void handle_get_sessions(ServerState *state, Client *client) {
    log_msg("PROTOCOL", "handle_get_sessions() - client %d", client->id);
    cJSON *response = create_sessions_list_json(state);
    
    char *json_str = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(response);
}

/**
 * Handles session creation request.
 * Validates parameters, creates session, adds creator as first player.
 * @param state Server state for session management
 * @param client Authenticated client creating the session
 * @param json Request body with name, themes, difficulty, etc.
 */
void handle_create_session(ServerState *state, Client *client, cJSON *json) {
    log_msg("PROTOCOL", "handle_create_session() - client %d ('%s')", 
           client->id, client->authenticated ? client->pseudo : "not auth");
    
    if (!client->authenticated) {
        log_msg("PROTOCOL", "handle_create_session() FAILED - not authenticated");
        send_error(client, "session/create", "401", "not authenticated");
        return;
    }
    
    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *theme_ids = cJSON_GetObjectItem(json, "themeIds");
    cJSON *difficulty = cJSON_GetObjectItem(json, "difficulty");
    cJSON *num_questions = cJSON_GetObjectItem(json, "nbQuestions");
    cJSON *time_limit = cJSON_GetObjectItem(json, "timeLimit");
    cJSON *mode = cJSON_GetObjectItem(json, "mode");
    cJSON *max_players = cJSON_GetObjectItem(json, "maxPlayers");
    cJSON *lives = cJSON_GetObjectItem(json, "lives");
    
    if (!name || !theme_ids || !difficulty || !num_questions || !time_limit || !mode || !max_players) {
        log_msg("PROTOCOL", "handle_create_session() FAILED - missing required fields");
        send_bad_request(client);
        return;
    }
    
    // lives is required for battle mode
    bool is_battle = strcmp(mode->valuestring, "battle") == 0;
    int initial_lives = 3; // default
    if (is_battle) {
        if (!lives || !cJSON_IsNumber(lives)) {
            log_msg("PROTOCOL", "handle_create_session() FAILED - lives required for battle mode");
            send_error(client, "session/create", "400", "lives required for battle mode");
            return;
        }
        initial_lives = lives->valueint;
        if (initial_lives < 1 || initial_lives > 10) {
            log_msg("PROTOCOL", "handle_create_session() FAILED - lives must be between 1 and 10");
            send_error(client, "session/create", "400", "lives must be between 1 and 10");
            return;
        }
    }
    
    log_msg("PROTOCOL", "Session params: name='%s', difficulty='%s', nbQ=%d, timeLimit=%d, mode='%s', lives=%d, maxPlayers=%d\\n",
           name->valuestring, difficulty->valuestring, num_questions->valueint, 
           time_limit->valueint, mode->valuestring, initial_lives, max_players->valueint);
    
    // Parse theme IDs
    int themes[MAX_THEMES];
    int num_themes = cJSON_GetArraySize(theme_ids);
    log_msg("PROTOCOL", "Parsing %d theme(s)", num_themes);
    for (int i = 0; i < num_themes && i < MAX_THEMES; i++) {
        themes[i] = cJSON_GetArrayItem(theme_ids, i)->valueint;
        log_msg("PROTOCOL", "  Theme ID: %d", themes[i]);
    }
    
    // Validate parameters
    int nb_q = num_questions->valueint;
    int t_limit = time_limit->valueint;
    int max_p = max_players->valueint;
    
    if (nb_q < 10 || nb_q > 50 || t_limit < 10 || t_limit > 60 || max_p < 2) {
        log_msg("PROTOCOL", "handle_create_session() FAILED - invalid parameters");
        send_error(client, "session/create", "400", "invalid parameters");
        return;
    }
    
    Session *session = create_session(state, 
        name->valuestring,
        themes, num_themes,
        string_to_difficulty(difficulty->valuestring),
        nb_q, t_limit,
        string_to_mode(mode->valuestring),
        initial_lives,
        max_p,
        client->id);
    
    if (!session) {
        log_msg("PROTOCOL", "handle_create_session() FAILED - not enough questions matching criteria");
        send_error(client, "session/create", "400", "not enough questions matching criteria");
        return;
    }
    
    log_msg("PROTOCOL", "Session created: id=%d, name='%s'", session->id, session->name);
    
    // Add creator to session
    join_session(state, session, client->id, client->pseudo);
    client->current_session_id = session->id;
    log_msg("PROTOCOL", "Creator '%s' joined session %d", client->pseudo, session->id);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "session/create");
    cJSON_AddStringToObject(response, "statut", "201");
    cJSON_AddStringToObject(response, "message", "session created");
    cJSON_AddNumberToObject(response, "sessionId", session->id);
    cJSON_AddBoolToObject(response, "isCreator", true);
    
    if (session->mode == MODE_BATTLE) {
        cJSON_AddNumberToObject(response, "lives", session->initial_lives);
    }
    
    cJSON *jokers = cJSON_AddObjectToObject(response, "jokers");
    cJSON_AddNumberToObject(jokers, "fifty", 1);
    cJSON_AddNumberToObject(jokers, "skip", 1);
    
    char *json_str = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(response);
}

/**
 * Handles session join request.
 * Validates session exists and has room, adds player to session.
 * @param state Server state for session lookup
 * @param client Authenticated client joining
 * @param json Request body with sessionId
 */
void handle_join_session(ServerState *state, Client *client, cJSON *json) {
    log_msg("PROTOCOL", "handle_join_session() - client %d ('%s')", 
           client->id, client->authenticated ? client->pseudo : "not auth");
    
    if (!client->authenticated) {
        log_msg("PROTOCOL", "handle_join_session() FAILED - not authenticated");
        send_error(client, "session/join", "401", "not authenticated");
        return;
    }
    
    cJSON *session_id = cJSON_GetObjectItem(json, "sessionId");
    if (!session_id || !cJSON_IsNumber(session_id)) {
        log_msg("PROTOCOL", "handle_join_session() FAILED - missing sessionId");
        send_bad_request(client);
        return;
    }
    
    log_msg("PROTOCOL", "Attempting to join session %d", session_id->valueint);
    
    Session *session = find_session(state, session_id->valueint);
    if (!session) {
        log_msg("PROTOCOL", "handle_join_session() FAILED - session not found");
        send_error(client, "session/join", "404", "session not found");
        return;
    }
    
    int result = join_session(state, session, client->id, client->pseudo);
    
    if (result == -2) {
        log_msg("PROTOCOL", "handle_join_session() FAILED - session is full");
        send_error(client, "session/join", "403", "session is full");
        return;
    } else if (result != 0) {
        log_msg("PROTOCOL", "handle_join_session() FAILED - cannot join (result=%d)", result);
        send_error(client, "session/join", "400", "cannot join session");
        return;
    }
    
    log_msg("PROTOCOL", "handle_join_session() SUCCESS - '%s' joined session %d", 
           client->pseudo, session->id);
    client->current_session_id = session->id;
    
    cJSON *response = create_session_join_response(session, client->id);
    
    char *json_str = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(response);
}

/**
 * Thread arguments for asynchronous session start.
 * Allows start_session to run without blocking main thread.
 */
typedef struct {
    ServerState *state;
    Session *session;
} StartSessionArgs;

/**
 * Thread function to start session asynchronously.
 * Frees arguments when done.
 * @param arg StartSessionArgs pointer
 * @return NULL
 */
static void* start_session_thread(void *arg) {
    log_msg("PROTOCOL", "start_session_thread() - thread started");
    StartSessionArgs *args = (StartSessionArgs*)arg;
    start_session(args->state, args->session);
    log_msg("PROTOCOL", "start_session_thread() - thread finished");
    free(args);
    return NULL;
}

/**
 * Handles session start request.
 * Validates creator and player count, spawns thread for game loop.
 * @param state Server state for session lookup
 * @param client Client requesting start (must be creator)
 */
void handle_start_session(ServerState *state, Client *client) {
    log_msg("PROTOCOL", "handle_start_session() - client %d, session_id=%d", 
           client->id, client->current_session_id);
    
    if (client->current_session_id < 0) {
        log_msg("PROTOCOL", "handle_start_session() FAILED - not in a session");
        send_error(client, "session/start", "400", "not in a session");
        return;
    }
    
    Session *session = find_session(state, client->current_session_id);
    if (!session) {
        log_msg("PROTOCOL", "handle_start_session() FAILED - session not found");
        send_error(client, "session/start", "404", "session not found");
        return;
    }
    
    if (session->creator_client_id != client->id) {
        log_msg("PROTOCOL", "handle_start_session() FAILED - not creator (creator=%d, requester=%d)\n",
               session->creator_client_id, client->id);
        send_error(client, "session/start", "403", "only creator can start session");
        return;
    }
    
    if (session->num_players < 2) {
        log_msg("PROTOCOL", "handle_start_session() FAILED - only %d player(s), need 2", 
               session->num_players);
        send_error(client, "session/start", "400", "need at least 2 players");
        return;
    }
    
    log_msg("PROTOCOL", "Starting session %d with %d players", session->id, session->num_players);
    
    // Start session in a new thread to not block
    pthread_t start_thread;
    
    StartSessionArgs *args = malloc(sizeof(StartSessionArgs));
    args->state = state;
    args->session = session;
    
    pthread_create(&start_thread, NULL, start_session_thread, args);
    pthread_detach(start_thread);
    log_msg("PROTOCOL", "Session start thread spawned");
}
