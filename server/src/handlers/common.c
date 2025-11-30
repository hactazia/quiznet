#include "handlers/common.h"
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
 * Sends a message to a specific client by ID.
 * Thread-safe, finds client in list and sends with newline terminator.
 * @param state Server state containing clients list
 * @param client_id Target client's unique ID
 * @param message JSON message string to send
 * @return Bytes sent on success, -1 if client not found
 */
int send_to_client(ServerState *state, int client_id, const char *message) {
    pthread_mutex_lock(&state->clients_mutex);
    
    for (int i = 0; i < state->num_clients; i++) {
        if (state->clients[i].id == client_id && state->clients[i].connected) {
            char buffer[MAX_MESSAGE_LEN + 4];
            int len = snprintf(buffer, sizeof(buffer), "%s\n", message);
            
            int result = send(state->clients[i].socket, buffer, len, 0);
            pthread_mutex_unlock(&state->clients_mutex);
            return result;
        }
    }
    
    pthread_mutex_unlock(&state->clients_mutex);
    return -1;
}

/**
 * Broadcasts a message to all players in a session.
 * Iterates through session players and sends to each.
 * @param state Server state for send_to_client
 * @param session Session containing player list
 * @param message JSON message string to broadcast
 * @return 0 always
 */
int broadcast_to_session(ServerState *state, Session *session, const char *message) {
    for (int i = 0; i < session->num_players; i++) {
        send_to_client(state, session->players[i].client_id, message);
    }
    return 0;
}

/**
 * Sends an error response to a client.
 * Creates JSON with action, status code, and error message.
 * @param client Client to send error to
 * @param action Optional action name for response
 * @param status HTTP-style status code as string
 * @param message Error description
 */
void send_error(Client *client, const char *action, const char *status, const char *message) {
    log_msg("PROTOCOL", "send_error() - action=%s, status=%s, message=%s", 
           action ? action : "null", status, message);
    cJSON *response = cJSON_CreateObject();
    if (action) {
        cJSON_AddStringToObject(response, "action", action);
    }
    cJSON_AddStringToObject(response, "statut", status);
    cJSON_AddStringToObject(response, "message", message);
    
    char *json = cJSON_PrintUnformatted(response);
    char buffer[MAX_MESSAGE_LEN];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", json);
    send(client->socket, buffer, len, 0);
    
    free(json);
    cJSON_Delete(response);
}

/**
 * Sends a 400 Bad Request error to the client.
 * Convenience wrapper for malformed requests.
 * @param client Client to send error to
 */
void send_bad_request(Client *client) {
    log_msg("PROTOCOL", "send_bad_request() to client %d", client->id);
    send_error(client, NULL, "400", "Bad request");
}

/**
 * Sends a 520 Unknown Error to the client.
 * Used for unrecognized endpoints or internal errors.
 * @param client Client to send error to
 */
void send_unknown_error(Client *client) {
    log_msg("PROTOCOL", "send_unknown_error() to client %d", client->id);
    send_error(client, NULL, "520", "Unknown Error");
}
