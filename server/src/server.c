#include "server.h"
#include "protocol.h"
#include "discover.h"
#include "session.h"
#include "player.h"
#include "question.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

typedef struct {
    ServerState *state;
    Client *client;
} ClientHandlerArgs;

/**
 * Initializes the server with TCP and UDP sockets.
 * Creates listening sockets, loads accounts and questions from data files.
 * @param state Server state structure to initialize
 * @param tcp_port Port number for TCP game connections
 * @param udp_port Port number for UDP server discovery
 * @return 0 on success, -1 on error
 */
int init_server(ServerState *state, int tcp_port, int udp_port) {
    log_msg("SERVER", "init_server() - initializing server on TCP:%d UDP:%d", tcp_port, udp_port);
    memset(state, 0, sizeof(ServerState));
    
    state->tcp_port = tcp_port;
    state->udp_port = udp_port;
    state->running = true;
    state->next_client_id = 1;
    state->next_session_id = 1;
    
    pthread_mutex_init(&state->clients_mutex, NULL);
    pthread_mutex_init(&state->sessions_mutex, NULL);
    pthread_mutex_init(&state->players_mutex, NULL);
    
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        log_msg("SERVER", "ERROR - WSAStartup failed");
        return -1;
    }
    log_msg("SERVER", "WSAStartup successful");
#endif
    
    state->tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (state->tcp_socket < 0) {
        log_msg("SERVER", "ERROR - Failed to create TCP socket");
        return -1;
    }
    log_msg("SERVER", "TCP socket created (fd=%d)", (int)state->tcp_socket);
    
    int opt = 1;
#ifdef _WIN32
    setsockopt(state->tcp_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(state->tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);
    
    if (bind(state->tcp_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        log_msg("SERVER", "ERROR - Failed to bind TCP socket to port %d", tcp_port);
        return -1;
    }

    log_msg("SERVER", "TCP socket bound to port %d", tcp_port);
    
    if (listen(state->tcp_socket, 10) < 0) {
        log_msg("SERVER", "ERROR - Failed to listen on TCP socket");
        return -1;
    }

    log_msg("SERVER", "TCP socket listening (backlog=10)");
    
    state->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (state->udp_socket < 0) {
        log_msg("SERVER", "ERROR - Failed to create UDP socket");
        return -1;
    }

    log_msg("SERVER", "UDP socket created (fd=%d)", (int)state->udp_socket);
    
#ifdef _WIN32
    setsockopt(state->udp_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(state->udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);
    
    if (bind(state->udp_socket, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        log_msg("SERVER", "ERROR - Failed to bind UDP socket to port %d", udp_port);
        return -1;
    }

    log_msg("SERVER", "UDP socket bound to port %d", udp_port);
    
    load_accounts(state);
    load_questions(state, NULL);
    
    log_msg("SERVER", "Server initialized successfully:");
    log_msg("SERVER", "  TCP port: %d", tcp_port);
    log_msg("SERVER", "  UDP port: %d", udp_port);
    log_msg("SERVER", "  Themes: %d", state->num_themes);
    log_msg("SERVER", "  Questions: %d", state->num_questions);
    log_msg("SERVER", "  Accounts: %d", state->num_players);
    
    return 0;
}

/**
 * Cleans up server resources and shuts down gracefully.
 * Closes all client connections, saves accounts, releases sockets.
 * @param state Server state to clean up
 */
void cleanup_server(ServerState *state) {
    log_msg("SERVER", "cleanup_server() - shutting down server");
    state->running = false;
    
    pthread_mutex_lock(&state->clients_mutex);
    log_msg("SERVER", "Closing %d client connections", state->num_clients);
    for (int i = 0; i < state->num_clients; i++) {
        if (state->clients[i].connected) {
            log_msg("SERVER", "Closing client %d socket", state->clients[i].id);
#ifdef _WIN32
            closesocket(state->clients[i].socket);
#else
            close(state->clients[i].socket);
#endif
        }
    }

    pthread_mutex_unlock(&state->clients_mutex);
    
    save_accounts(state);
    
    if (state->tcp_socket > 0) {
#ifdef _WIN32
        closesocket(state->tcp_socket);
        closesocket(state->udp_socket);
        WSACleanup();
#else
        close(state->tcp_socket);
        close(state->udp_socket);
#endif
        state->tcp_socket = 0;
        state->udp_socket = 0;
    }
#ifdef _WIN32
    else {
        WSACleanup();
    }
#endif
    
    pthread_mutex_destroy(&state->clients_mutex);
    pthread_mutex_destroy(&state->sessions_mutex);
    pthread_mutex_destroy(&state->players_mutex);
    
    log_msg("SERVER", "Server cleaned up successfully");
}

/**
 * Accepts a new TCP client connection and initializes client structure.
 * Thread-safe, finds empty slot in clients array.
 * @param state Server state containing clients array
 * @return Pointer to the new Client structure, NULL if max clients reached
 */
Client* accept_client(ServerState *state) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_socket = accept(state->tcp_socket, (struct sockaddr*)&client_addr, &addr_len);
    if (client_socket < 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&state->clients_mutex);
    
    if (state->num_clients >= MAX_CLIENTS) {
        pthread_mutex_unlock(&state->clients_mutex);
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
        return NULL;
    }
    
    Client *client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!state->clients[i].connected) {
            client = &state->clients[i];
            break;
        }
    }
    
    if (!client) {
        client = &state->clients[state->num_clients];
    }
    
    memset(client, 0, sizeof(Client));
    client->id = state->next_client_id++;
    client->socket = client_socket;
    client->connected = true;
    client->authenticated = false;
    client->current_session_id = -1;
    strncpy(client->ip, inet_ntoa(client_addr.sin_addr), 15);
    client->port = ntohs(client_addr.sin_port);
    
    state->num_clients++;
    
    pthread_mutex_unlock(&state->clients_mutex);
    
    log_msg("SERVER", "Client connected: %s:%d (ID: %d, total clients: %d)", 
           client->ip, client->port, client->id, state->num_clients);
    
    return client;
}

/**
 * Disconnects a client and cleans up their resources.
 * Removes client from any active session, closes socket.
 * @param state Server state
 * @param client Client to disconnect
 */
void disconnect_client(ServerState *state, Client *client) {
    log_msg("SERVER", "Client disconnecting: %s:%d (ID: %d, pseudo: %s)", 
           client->ip, client->port, client->id, 
           client->authenticated ? client->pseudo : "<not authenticated>");
    
    if (client->current_session_id > 0) {
        log_msg("SERVER", "Client was in session %d, leaving...", client->current_session_id);
        Session *session = find_session(state, client->current_session_id);
        if (session) {
            leave_session(state, session, client->id);
        }
    }
    
#ifdef _WIN32
    closesocket(client->socket);
#else
    close(client->socket);
#endif
    
    pthread_mutex_lock(&state->clients_mutex);
    client->connected = false;
    state->num_clients--;
    log_msg("SERVER", "Client disconnected (remaining clients: %d)", state->num_clients);
    pthread_mutex_unlock(&state->clients_mutex);
}

/**
 * Thread handler for processing client messages.
 * Implements the two-line protocol (METHOD path\n{json}).
 * Runs until client disconnects or server stops.
 * @param arg Pointer to ClientHandlerArgs containing state and client
 * @return NULL when thread exits
 */
void* client_handler(void *arg) {
    ClientHandlerArgs *args = (ClientHandlerArgs*)arg;
    ServerState *state = args->state;
    Client *client = args->client;
    free(args);
    
    log_msg("CLIENT", "Handler started for client %d (%s:%d)", client->id, client->ip, client->port);
    
    char buffer[MAX_MESSAGE_LEN];
    char message_buffer[MAX_MESSAGE_LEN * 2] = "";
    int message_len = 0;
    
    char pending_request[MAX_MESSAGE_LEN] = "";
    int expecting_json = 0;
    
    while (client->connected && state->running) {
        int received = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0) {
            log_msg("CLIENT", "Client %d: recv() returned %d, closing connection", client->id, received);
            break;
        }
        
        buffer[received] = '\0';
        log_msg("CLIENT", "Client %d: Received %d bytes", client->id, received);
        
        // Append to message buffer
        if (message_len + received < (int)sizeof(message_buffer) - 1) {
            strcat(message_buffer, buffer);
            message_len += received;
        }
        
        char *newline;
        while ((newline = strchr(message_buffer, '\n')) != NULL) {
            *newline = '\0';
            
            if (strlen(message_buffer) > 0) {
                log_msg("CLIENT", "Client %d: Line: '%s'", client->id, message_buffer);
                
                if (expecting_json) {
                    char full_request[MAX_MESSAGE_LEN * 2];
                    snprintf(full_request, sizeof(full_request), "%s\n%s", pending_request, message_buffer);
                    handle_request(state, client, full_request);
                    expecting_json = 0;
                    pending_request[0] = '\0';
                } else if (strncmp(message_buffer, "GET ", 4) == 0) {
                    log_msg("CLIENT", "Client %d: GET request detected", client->id);
                    handle_request(state, client, message_buffer);
                } else if (strncmp(message_buffer, "POST ", 5) == 0) {
                    log_msg("CLIENT", "Client %d: POST request detected, waiting for JSON body", client->id);
                    strncpy(pending_request, message_buffer, MAX_MESSAGE_LEN - 1);
                    expecting_json = 1;
                } else {
                    log_msg("CLIENT", "Client %d: Unknown format, processing as-is", client->id);
                    handle_request(state, client, message_buffer);
                }
            }
            
            char *remaining = newline + 1;
            memmove(message_buffer, remaining, strlen(remaining) + 1);
            message_len = strlen(message_buffer);
        }
    }
    
    log_msg("CLIENT", "Client %d: Handler ending", client->id);
    disconnect_client(state, client);
    return NULL;
}

/**
 * Main server loop that accepts connections and spawns client handlers.
 * Starts UDP discovery thread, then loops accepting TCP connections.
 * @param state Server state
 */
void run_server(ServerState *state) {
    log_msg("SERVER", "run_server() - server starting main loop");
    
    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, udp_discovery_handler, state);
    state->udp_thread = udp_thread;
    
    log_msg("SERVER", "Waiting for connections on port %d...", state->tcp_port);
    
    while (state->running) {
        Client *client = accept_client(state);
        if (client) {
            log_msg("SERVER", "Spawning handler thread for client %d", client->id);
            ClientHandlerArgs *args = malloc(sizeof(ClientHandlerArgs));
            args->state = state;
            args->client = client;
            
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, client_handler, args);
            pthread_detach(client_thread);
        }
    }
    
    log_msg("SERVER", "run_server() - main loop ended, canceling UDP thread");
    pthread_cancel(udp_thread);
    pthread_join(udp_thread, NULL);
    log_msg("SERVER", "run_server() - UDP thread joined");
}

/**
 * Signals the server to stop accepting connections and shut down.
 * Sets the running flag to false and closes sockets to unblock accept/recvfrom.
 * @param state Server state
 */
void stop_server(ServerState *state) {
    log_msg("SERVER", "stop_server() - stopping server");
    state->running = false;
    
    if (state->tcp_socket > 0) {
#ifdef _WIN32
        closesocket(state->tcp_socket);
        closesocket(state->udp_socket);
#else
        close(state->tcp_socket);
        close(state->udp_socket);
#endif
        state->tcp_socket = 0;
        state->udp_socket = 0;
    }
}
