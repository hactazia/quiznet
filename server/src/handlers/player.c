#include "handlers/player.h"
#include "handlers/common.h"
#include "player.h"
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
 * Handles player registration request.
 * Validates pseudo/password, creates new account if unique.
 * @param state Server state with accounts list
 * @param client Client making the request
 * @param json Request body with pseudo and password
 */
void handle_register(ServerState *state, Client *client, cJSON *json) {
    log_msg("PROTOCOL", "handle_register() - client %d", client->id);
    cJSON *pseudo = cJSON_GetObjectItem(json, "pseudo");
    cJSON *password = cJSON_GetObjectItem(json, "password");
    
    if (!pseudo || !password || !cJSON_IsString(pseudo) || !cJSON_IsString(password)) {
        log_msg("PROTOCOL", "handle_register() FAILED - missing or invalid pseudo/password");
        send_bad_request(client);
        return;
    }
    
    log_msg("PROTOCOL", "handle_register() - pseudo='%s'", pseudo->valuestring);
    int result = register_player(state, pseudo->valuestring, password->valuestring);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "player/register");
    
    if (result == 0) {
        log_msg("PROTOCOL", "handle_register() SUCCESS - player registered");
        cJSON_AddStringToObject(response, "statut", "201");
        cJSON_AddStringToObject(response, "message", "player registered successfully");
    } else {
        log_msg("PROTOCOL", "handle_register() FAILED - pseudo already exists (result=%d)", result);
        cJSON_AddStringToObject(response, "statut", "409");
        cJSON_AddStringToObject(response, "message", "pseudo already exists");
    }
    
    char *json_str = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(response);
}

/**
 * Handles player login request.
 * Validates credentials, marks client as authenticated on success.
 * @param state Server state with accounts list
 * @param client Client making the request
 * @param json Request body with pseudo and password
 */
void handle_login(ServerState *state, Client *client, cJSON *json) {
    log_msg("PROTOCOL", "handle_login() - client %d", client->id);
    cJSON *pseudo = cJSON_GetObjectItem(json, "pseudo");
    cJSON *password = cJSON_GetObjectItem(json, "password");
    
    if (!pseudo || !password || !cJSON_IsString(pseudo) || !cJSON_IsString(password)) {
        log_msg("PROTOCOL", "handle_login() FAILED - missing or invalid pseudo/password");
        send_bad_request(client);
        return;
    }
    
    log_msg("PROTOCOL", "handle_login() - attempting login for pseudo='%s'", pseudo->valuestring);
    int result = login_player(state, pseudo->valuestring, password->valuestring);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "player/login");
    
    if (result == 0) {
        log_msg("PROTOCOL", "handle_login() SUCCESS - '%s' logged in", pseudo->valuestring);
        cJSON_AddStringToObject(response, "statut", "200");
        cJSON_AddStringToObject(response, "message", "login successful");
        
        strncpy(client->pseudo, pseudo->valuestring, MAX_PSEUDO_LEN - 1);
        client->authenticated = true;
    } else {
        log_msg("PROTOCOL", "handle_login() FAILED - invalid credentials");
        cJSON_AddStringToObject(response, "statut", "401");
        cJSON_AddStringToObject(response, "message", "invalid credentials");
    }
    
    char *json_str = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json_str);
    send(client->socket, buffer, len, 0);
    
    free(json_str);
    cJSON_Delete(response);
}
