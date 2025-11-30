#include "player.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#define ACCOUNTS_FILE "data/accounts.dat"
#define NOT_FOUND -1
#define TOO_MANY_ACCOUNTS -2
#define MAX_PSEUDO_LEN 32

/**
 * Registers a new player account with the given credentials.
 * The password is hashed before storage. Thread-safe.
 * @param state Server state containing accounts array
 * @param pseudo The username for the new account
 * @param password The password (will be hashed)
 * @return 0 on success, -1 if pseudo exists, -2 if max accounts reached
 */
int register_player(ServerState *state, const char *pseudo, const char *password) {
    log_msg("PLAYER", "register_player() called - pseudo='%s'", pseudo);
    
    pthread_mutex_lock(&state->accounts_mutex);
    
    for (int i = 0; i < state->num_accounts; i++) {
        if (strcmp(state->accounts[i].pseudo, pseudo) == 0) {
            log_msg("PLAYER", "register_player() FAILED - pseudo '%s' already exists", pseudo);
            pthread_mutex_unlock(&state->accounts_mutex);
            return NOT_FOUND;
        }
    }
    
    if (state->num_accounts >= MAX_CLIENTS) {
        log_msg("PLAYER", "register_player() FAILED - max accounts reached (%d)", MAX_CLIENTS);
        pthread_mutex_unlock(&state->accounts_mutex);
        return TOO_MANY_ACCOUNTS;
    }
    
    PlayerAccount *account = &state->accounts[state->num_accounts];
    account->id = state->num_accounts;
    strncpy(account->pseudo, pseudo, MAX_PSEUDO_LEN - 1);
    account->pseudo[MAX_PSEUDO_LEN - 1] = '\0';
    sha256_hash(password, account->password_hash);
    account->logged_in = false;
    
    state->num_accounts++;
    log_msg("PLAYER", "register_player() SUCCESS - new account id=%d, total=%d", account->id, state->num_accounts);
    
    pthread_mutex_unlock(&state->accounts_mutex);

    save_accounts(state);
    
    return 0;
}

/**
 * Authenticates a player with the given credentials.
 * Compares password hash against stored hash. Thread-safe.
 * @param state Server state containing accounts array
 * @param pseudo The username to authenticate
 * @param password The password to verify
 * @return 0 on success, -1 on invalid credentials
 */
int login_player(ServerState *state, const char *pseudo, const char *password) {
    log_msg("PLAYER", "login_player() called - pseudo='%s'", pseudo);

    pthread_mutex_lock(&state->accounts_mutex);
    
    char password_hash[65];
    sha256_hash(password, password_hash);
    
    for (int i = 0; i < state->num_accounts; i++) {
        if (strcmp(state->accounts[i].pseudo, pseudo) == 0) {
            if (strcmp(state->accounts[i].password_hash, password_hash) == 0) {
                state->accounts[i].logged_in = true;
                log_msg("PLAYER", "login_player() SUCCESS - '%s' logged in", pseudo);
                pthread_mutex_unlock(&state->accounts_mutex);
                return 0;
            }
            log_msg("PLAYER", "login_player() FAILED - wrong password for '%s'", pseudo);
            pthread_mutex_unlock(&state->accounts_mutex);
            return -1;
        }
    }
    
    log_msg("PLAYER", "login_player() FAILED - player '%s' not found", pseudo);
    pthread_mutex_unlock(&state->accounts_mutex);
    return NOT_FOUND;
}

/**
 * Finds a player account by their username.
 * @param state Server state containing accounts array
 * @param pseudo The username to search for
 * @return Pointer to the PlayerAccount if found, NULL otherwise
 */
PlayerAccount* find_player_by_pseudo(ServerState *state, const char *pseudo) {
    log_msg("PLAYER", "find_player_by_pseudo() - searching for '%s'", pseudo);

    for (int i = 0; i < state->num_accounts; i++) {
        if (strcmp(state->accounts[i].pseudo, pseudo) == 0) {
            log_msg("PLAYER", "find_player_by_pseudo() - FOUND at index %d", i);
            return &state->accounts[i];
        }
    }
    log_msg("PLAYER", "find_player_by_pseudo() - NOT FOUND");
    return NULL;
}

/**
 * Loads all player accounts from the data file into memory.
 * Creates an empty accounts list if file doesn't exist.
 * @param state Server state to populate with loaded accounts
 * @return Number of accounts loaded, 0 if file not found
 */
int load_accounts(ServerState *state) {
    log_msg("PLAYER", "load_accounts() - opening %s...", ACCOUNTS_FILE);

    FILE *file = fopen(ACCOUNTS_FILE, "r");
    if (!file) {
        log_msg("PLAYER", "load_accounts() - No accounts file found, starting fresh");
        return 0;
    }
    
    char line[256];
    state->num_accounts = 0;
    
    while (fgets(line, sizeof(line), file) && state->num_accounts < MAX_CLIENTS) {
        trim_whitespace(line);
        if (strlen(line) == 0) continue;
        
        char pseudo[MAX_PSEUDO_LEN];
        char hash[65];
        
        if (sscanf(line, "%31[^;];%64s", pseudo, hash) == 2) {
            PlayerAccount *account = &state->accounts[state->num_accounts];
            account->id = state->num_accounts;
            strncpy(account->pseudo, pseudo, MAX_PSEUDO_LEN - 1);
            strncpy(account->password_hash, hash, 64);
            account->logged_in = false;
            log_msg("PLAYER", "load_accounts() - loaded account: id=%d, pseudo='%s'", account->id, pseudo);
            state->num_accounts++;
        }
    }
    
    fclose(file);
    log_msg("PLAYER", "load_accounts() - Total loaded: %d accounts", state->num_accounts);
    return state->num_accounts;
}

/**
 * Saves all player accounts from memory to the data file.
 * Thread-safe, uses mutex to protect account data during write.
 * @param state Server state containing accounts to save
 * @return 0 on success, -1 on file write error
 */
int save_accounts(ServerState *state) {
    log_msg("PLAYER", "save_accounts() - saving %d accounts to %s", state->num_accounts, ACCOUNTS_FILE);

    FILE *file = fopen(ACCOUNTS_FILE, "w");
    if (!file) {
        log_msg("PLAYER", "save_accounts() ERROR - Failed to open file for writing");
        perror("Failed to save accounts");
        return -1;
    }
    
    pthread_mutex_lock(&state->accounts_mutex);
    
    for (int i = 0; i < state->num_accounts; i++) {
        fprintf(file, "%s;%s\n", 
                state->accounts[i].pseudo, 
                state->accounts[i].password_hash);
    }
    
    pthread_mutex_unlock(&state->accounts_mutex);
    
    fclose(file);
    log_msg("PLAYER", "save_accounts() - SUCCESS");
    return 0;
}
