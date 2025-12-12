#include "session.h"
#include "question.h"
#include "protocol.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * Creates a new game session with specified parameters.
 * Initializes session structure, selects matching questions, and registers in server state.
 * @param state Server state containing sessions array and questions
 * @param name Display name for the session
 * @param theme_ids Array of theme IDs to filter questions
 * @param num_themes Number of themes in the array
 * @param difficulty Difficulty level for question filtering
 * @param num_questions Number of questions for the game
 * @param time_limit Time allowed per question in seconds
 * @param mode Game mode (solo or battle)
 * @param max_players Maximum number of players allowed
 * @param creator_client_id Client ID of the session creator
 * @return Pointer to created session, or NULL on failure
 */
Session* create_session(ServerState *state, const char *name, int *theme_ids, int num_themes,
                        Difficulty difficulty, int num_questions, int time_limit,
                        GameMode mode, int initial_lives, int max_players, int creator_client_id) {
    log_msg("SESSION", "create_session() - name='%s', themes=%d, difficulty=%d, questions=%d",
           name, num_themes, difficulty, num_questions);
    pthread_mutex_lock(&state->sessions_mutex);
    
    if (state->num_sessions >= MAX_SESSIONS) {
        log_msg("SESSION", "create_session() FAILED - max sessions reached (%d)", MAX_SESSIONS);
        pthread_mutex_unlock(&state->sessions_mutex);
        return NULL;
    }
    
    Session *session = NULL;
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (state->sessions[i].status == SESSION_FINISHED || state->sessions[i].id == 0) {
            session = &state->sessions[i];
            log_msg("SESSION", "Found empty slot at index %d", i);
            break;
        }
    }
    
    if (!session) {
        log_msg("SESSION", "create_session() FAILED - no empty slot found");
        pthread_mutex_unlock(&state->sessions_mutex);
        return NULL;
    }
    
    memset(session, 0, sizeof(Session));
    pthread_mutex_init(&session->mutex, NULL);
    
    session->id = state->next_session_id++;
    strncpy(session->name, name, 63);
    
    session->num_themes = num_themes;
    for (int i = 0; i < num_themes; i++) {
        session->theme_ids[i] = theme_ids[i];
    }
    
    session->difficulty = difficulty;
    session->num_questions = num_questions;
    session->time_limit = time_limit;
    session->mode = mode;
    session->initial_lives = (mode == MODE_BATTLE) ? initial_lives : 0;
    session->max_players = max_players;
    session->status = SESSION_WAITING;
    session->creator_client_id = creator_client_id;
    session->current_question = -1;
    
    log_msg("SESSION", "Session initialized: id=%d, selecting questions...", session->id);
    
    if (select_questions_for_session(state, session) < 0) {
        log_msg("SESSION", "create_session() FAILED - not enough matching questions");
        memset(session, 0, sizeof(Session));
        pthread_mutex_unlock(&state->sessions_mutex);
        return NULL;
    }
    
    state->num_sessions++;
    log_msg("SESSION", "Session created successfully: id=%d (total sessions: %d)", 
           session->id, state->num_sessions);
    
    pthread_mutex_unlock(&state->sessions_mutex);
    
    return session;
}

/**
 * Finds a session by its unique ID.
 * Searches through all session slots in server state.
 * @param state Server state containing sessions array
 * @param session_id Unique session ID to find
 * @return Pointer to session if found, NULL otherwise
 */
Session* find_session(ServerState *state, int session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (state->sessions[i].id == session_id) {
            return &state->sessions[i];
        }
    }
    log_msg("SESSION", "find_session() - session %d not found", session_id);
    return NULL;
}

/**
 * Adds a player to an existing session.
 * Validates session is waiting and has room, notifies other players.
 * @param state Server state for sending notifications
 * @param session Target session to join
 * @param client_id Client ID of the joining player
 * @param pseudo Display name of the joining player
 * @return 0 on success, -1 not waiting, -2 full, -3 already in session
 */
int join_session(ServerState *state, Session *session, int client_id, const char *pseudo) {
    log_msg("SESSION", "join_session() - client %d ('%s') joining session %d",
           client_id, pseudo, session->id);
    pthread_mutex_lock(&session->mutex);
    
    if (session->status != SESSION_WAITING) {
        log_msg("SESSION", "join_session() FAILED - session not waiting (status=%d)", session->status);
        pthread_mutex_unlock(&session->mutex);
        return -1;
    }
    
    if (session->num_players >= session->max_players) {
        log_msg("SESSION", "join_session() FAILED - session full (%d/%d)", 
               session->num_players, session->max_players);
        pthread_mutex_unlock(&session->mutex);
        return -2;
    }
    
    // Check if already in session
    for (int i = 0; i < session->num_players; i++) {
        if (session->players[i].client_id == client_id) {
            log_msg("SESSION", "join_session() FAILED - already in session");
            pthread_mutex_unlock(&session->mutex);
            return -3;
        }
    }
    
    SessionPlayer *player = &session->players[session->num_players];
    player->client_id = client_id;
    strncpy(player->pseudo, pseudo, MAX_PSEUDO_LEN - 1);
    player->score = 0;
    player->lives = session->initial_lives;
    player->correct_answers = 0;
    player->has_answered = false;
    player->current_answer = -1;
    player->response_time = 0;
    player->eliminated = false;
    player->eliminated_at = 0;
    player->joker_fifty_used = false;
    player->joker_skip_used = false;
    player->used_skip_this_question = false;
    
    session->num_players++;
    log_msg("SESSION", "Player '%s' added (now %d/%d players)", 
           pseudo, session->num_players, session->max_players);
    
    log_msg("SESSION", "Notifying %d other player(s)", session->num_players - 1);
    for (int i = 0; i < session->num_players - 1; i++) {
        int other_client_id = session->players[i].client_id;
        
        cJSON *notify = cJSON_CreateObject();
        cJSON_AddStringToObject(notify, "action", "session/player/joined");
        cJSON_AddStringToObject(notify, "pseudo", pseudo);
        cJSON_AddNumberToObject(notify, "nbPlayers", session->num_players);
        
        char *msg = cJSON_PrintUnformatted(notify);
        send_to_client(state, other_client_id, msg);
        free(msg);
        cJSON_Delete(notify);
    }
    
    pthread_mutex_unlock(&session->mutex);
    return 0;
}

/**
 * Removes a player from a session.
 * Shifts remaining players, reassigns creator if needed, notifies others.
 * @param state Server state for sending notifications
 * @param session Session to leave
 * @param client_id Client ID of the leaving player
 * @return 0 on success, -1 if player not in session
 */
int leave_session(ServerState *state, Session *session, int client_id) {
    log_msg("SESSION", "leave_session() - client %d leaving session %d", client_id, session->id);
    pthread_mutex_lock(&session->mutex);
    
    int player_index = -1;
    char leaving_pseudo[MAX_PSEUDO_LEN] = "";
    
    for (int i = 0; i < session->num_players; i++) {
        if (session->players[i].client_id == client_id) {
            player_index = i;
            strncpy(leaving_pseudo, session->players[i].pseudo, MAX_PSEUDO_LEN - 1);
            break;
        }
    }
    
    if (player_index < 0) {
        log_msg("SESSION", "leave_session() FAILED - client not in session");
        pthread_mutex_unlock(&session->mutex);
        return -1;
    }
    
    log_msg("SESSION", "Removing player '%s' at index %d", leaving_pseudo, player_index);
    
    for (int i = player_index; i < session->num_players - 1; i++) {
        session->players[i] = session->players[i + 1];
    }
    session->num_players--;
    
    if (client_id == session->creator_client_id && session->num_players > 0) {
        session->creator_client_id = session->players[0].client_id;
        log_msg("SESSION", "New creator: client %d ('%s')", 
               session->creator_client_id, session->players[0].pseudo);
    }
    
    log_msg("SESSION", "Notifying %d remaining player(s)", session->num_players);
    for (int i = 0; i < session->num_players; i++) {
        int other_client_id = session->players[i].client_id;
        
        cJSON *notify = cJSON_CreateObject();
        cJSON_AddStringToObject(notify, "action", "session/player/left");
        cJSON_AddStringToObject(notify, "pseudo", leaving_pseudo);
        cJSON_AddStringToObject(notify, "reason", "disconnected");
        
        char *msg = cJSON_PrintUnformatted(notify);
        send_to_client(state, other_client_id, msg);
        free(msg);
        cJSON_Delete(notify);
    }
    
    if (session->num_players == 0) {
        log_msg("SESSION", "No players left, ending session");
        session->status = SESSION_FINISHED;
        pthread_mutex_unlock(&session->mutex);
    } else if (session->num_players == 1 && session->status == SESSION_PLAYING) {
        log_msg("SESSION", "Only 1 player left during game, ending session with results");
        pthread_mutex_unlock(&session->mutex);
        end_session(state, session);
    } else {
        pthread_mutex_unlock(&session->mutex);
    }
    return 0;
}

/**
 * Starts a game session after countdown.
 * Validates minimum players, sends start notification, then first question.
 * @param state Server state for sending messages
 * @param session Session to start
 * @return 0 on success, -1 if not enough players
 */
int start_session(ServerState *state, Session *session) {
    log_msg("SESSION", "start_session() - session %d starting with %d players", 
           session->id, session->num_players);
    pthread_mutex_lock(&session->mutex);
    
    if (session->num_players < 2) {
        log_msg("SESSION", "start_session() FAILED - not enough players");
        pthread_mutex_unlock(&session->mutex);
        return -1;
    }
    
    session->status = SESSION_PLAYING;
    session->current_question = 0;
    log_msg("SESSION", "Session status set to PLAYING, starting with question 0");
    
    log_msg("SESSION", "Sending start notification to %d players", session->num_players);
    for (int i = 0; i < session->num_players; i++) {
        cJSON *notify = cJSON_CreateObject();
        cJSON_AddStringToObject(notify, "action", "session/started");
        cJSON_AddStringToObject(notify, "message", "session is starting");
        cJSON_AddNumberToObject(notify, "countdown", 3);
        
        char *msg = cJSON_PrintUnformatted(notify);
        send_to_client(state, session->players[i].client_id, msg);
        free(msg);
        cJSON_Delete(notify);
    }
    
    pthread_mutex_unlock(&session->mutex);
    
    log_msg("SESSION", "Waiting 3 seconds countdown...");
    #ifdef _WIN32
    Sleep(3000);
    #else
    sleep(3);
    #endif
    
    // Send first question
    log_msg("SESSION", "Sending first question");
    send_question_to_all(state, session);
    
    return 0;
}

/**
 * Finds a player in a session by client ID.
 * Searches through session's player list.
 * @param session Session to search in
 * @param client_id Client ID to find
 * @return Pointer to SessionPlayer if found, NULL otherwise
 */
SessionPlayer* find_session_player(Session *session, int client_id) {
    for (int i = 0; i < session->num_players; i++) {
        if (session->players[i].client_id == client_id) {
            return &session->players[i];
        }
    }
    log_msg("SESSION", "find_session_player() - client %d not found", client_id);
    return NULL;
}

/**
 * Finds a player in a session by their pseudo/username.
 * Case-sensitive string comparison.
 * @param session Session to search in
 * @param pseudo Player's display name to find
 * @return Pointer to SessionPlayer if found, NULL otherwise
 */
SessionPlayer* find_session_player_by_pseudo(Session *session, const char *pseudo) {
    for (int i = 0; i < session->num_players; i++) {
        if (strcmp(session->players[i].pseudo, pseudo) == 0) {
            return &session->players[i];
        }
    }
    return NULL;
}

/**
 * Gets the current question being asked in a session.
 * Uses session's question_ids array and current_question index.
 * @param state Server state containing all questions
 * @param session Session with current question index
 * @return Pointer to current Question, NULL if out of range
 */
Question* get_current_question(ServerState *state, Session *session) {
    if (session->current_question < 0 || session->current_question >= session->num_questions) {
        return NULL;
    }
    
    int question_id = session->question_ids[session->current_question];
    
    for (int i = 0; i < state->num_questions; i++) {
        if (state->questions[i].id == question_id) {
            return &state->questions[i];
        }
    }
    return NULL;
}

/**
 * Sends the current question to all active players.
 * Resets player answer states, formats question as JSON.
 * @param state Server state for sending messages
 * @param session Session with current question
 */
void send_question_to_all(ServerState *state, Session *session) {
    pthread_mutex_lock(&session->mutex);
    
    Question *q = get_current_question(state, session);
    if (!q) {
        log_msg("SESSION", "send_question_to_all() FAILED - no current question");
        pthread_mutex_unlock(&session->mutex);
        return;
    }
    
    log_msg("SESSION", "Sending question %d/%d: '%s'", 
           session->current_question + 1, session->num_questions, q->question);
    
    for (int i = 0; i < session->num_players; i++) {
        session->players[i].has_answered = false;
        session->players[i].was_correct = false;
        session->players[i].current_answer = -1;
        session->players[i].response_time = 0;
        session->players[i].used_skip_this_question = false;
    }
    
    session->question_start_time = time(NULL);
    
    int active_players = 0;
    for (int i = 0; i < session->num_players; i++) {
        if (session->players[i].eliminated) {
            log_msg("SESSION", "  Skipping eliminated player '%s'", session->players[i].pseudo);
            continue;
        }
        active_players++;
        
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "action", "question/new");
        cJSON_AddNumberToObject(msg, "questionNum", session->current_question + 1);
        cJSON_AddNumberToObject(msg, "totalQuestions", session->num_questions);
        cJSON_AddStringToObject(msg, "type", question_type_to_string(q->type));
        cJSON_AddStringToObject(msg, "difficulty", difficulty_to_string(q->difficulty));
        cJSON_AddStringToObject(msg, "question", q->question);
        cJSON_AddNumberToObject(msg, "timeLimit", session->time_limit);
        
        if (q->type == QUESTION_QCM) {
            cJSON *answers = cJSON_AddArrayToObject(msg, "answers");
            for (int j = 0; j < 4; j++) {
                cJSON_AddItemToArray(answers, cJSON_CreateString(q->answers[j]));
            }
        }
        
        char *json = cJSON_PrintUnformatted(msg);
        send_to_client(state, session->players[i].client_id, json);
        free(json);
        cJSON_Delete(msg);
    }
    
    log_msg("SESSION", "Question sent to %d active player(s)", active_players);
    pthread_mutex_unlock(&session->mutex);
}

/**
 * Processes a player's answer to the current question.
 * Validates timing, checks correctness, awards points, triggers results when all answered.
 * @param state Server state for sending results
 * @param session Current game session
 * @param client_id Client who submitted the answer
 * @param answer_index QCM answer index (0-3)
 * @param text_answer Text answer for TEXT type questions
 * @param bool_answer Boolean answer for BOOLEAN type questions
 * @param response_time Time taken to answer in seconds
 */
void process_answer(ServerState *state, Session *session, int client_id,
                   int answer_index, const char *text_answer, bool bool_answer, double response_time) {
    log_msg("SESSION", "process_answer() - client %d, answer=%d, time=%.2f", 
           client_id, answer_index, response_time);
    pthread_mutex_lock(&session->mutex);
    
    SessionPlayer *player = find_session_player(session, client_id);
    if (!player || player->has_answered || player->eliminated) {
        pthread_mutex_unlock(&session->mutex);
        return;
    }
    
    time_t current_time = time(NULL);
    double server_elapsed = difftime(current_time, session->question_start_time);
    
    if (server_elapsed > session->time_limit + 1) { // 1 second grace period
        response_time = session->time_limit + 1;
    }
    
    player->has_answered = true;
    player->current_answer = answer_index;
    player->response_time = response_time;
    
    Question *q = get_current_question(state, session);
    bool correct = false;
    
    if (q) {
        if (q->type == QUESTION_TEXT) {
            correct = check_answer(q, 0, text_answer, false);
        } else if (q->type == QUESTION_BOOLEAN) {
            correct = check_answer(q, 0, NULL, bool_answer);
            player->current_answer = bool_answer ? 1 : 0;
        } else {
            correct = check_answer(q, answer_index, NULL, false);
        }
        
        if (correct) {
            int points = calculate_points(q->difficulty, response_time, session->time_limit);
            player->score += points;
            player->correct_answers++;
        }
        player->was_correct = correct;
    }
    
    bool all_answered = true;
    for (int i = 0; i < session->num_players; i++) {
        if (!session->players[i].eliminated && !session->players[i].has_answered) {
            all_answered = false;
            break;
        }
    }
    
    pthread_mutex_unlock(&session->mutex);
    
    if (all_answered) {
        send_question_results(state, session);
    }
}

/**
 * Sends question results to all players after everyone answered.
 * Applies Battle mode penalties, builds results JSON, checks for game end.
 * @param state Server state for sending messages
 * @param session Current game session
 */
void send_question_results(ServerState *state, Session *session) {
    pthread_mutex_lock(&session->mutex);
    
    Question *q = get_current_question(state, session);
    if (!q) {
        pthread_mutex_unlock(&session->mutex);
        return;
    }
    
    double max_response_time = 0;
    int last_player_index = -1;
    
    if (session->mode == MODE_BATTLE) {
        for (int i = 0; i < session->num_players; i++) {
            SessionPlayer *p = &session->players[i];
            if (p->eliminated || p->used_skip_this_question) continue;
            
            bool correct = false;
            if (q->type == QUESTION_QCM) {
                correct = (p->current_answer == q->correct_answer);
            } else if (q->type == QUESTION_BOOLEAN) {
                correct = (p->current_answer == q->correct_answer);
            } else {
                correct = p->has_answered;
            }
            
            if (!correct && p->has_answered) {
                p->lives--;
                if (p->lives <= 0) {
                    p->eliminated = true;
                    p->eliminated_at = session->current_question + 1;
                }
            }
            
            if (p->has_answered && p->response_time > max_response_time) {
                max_response_time = p->response_time;
                last_player_index = i;
            }
        }
        
        if (last_player_index >= 0) {
            SessionPlayer *last = &session->players[last_player_index];
            if (!last->eliminated) {
                bool was_correct = false;
                if (q->type == QUESTION_QCM || q->type == QUESTION_BOOLEAN) {
                    was_correct = (last->current_answer == q->correct_answer);
                }
                if (was_correct) { 
                    last->lives--;
                    if (last->lives <= 0) {
                        last->eliminated = true;
                        last->eliminated_at = session->current_question + 1;
                    }
                }
            }
        }
    }
    
    cJSON *results = cJSON_CreateObject();
    cJSON_AddStringToObject(results, "action", "question/results");
    
    if (q->type == QUESTION_QCM || q->type == QUESTION_BOOLEAN) {
        cJSON_AddNumberToObject(results, "correctAnswer", q->correct_answer);
    } else {
        cJSON_AddStringToObject(results, "correctAnswer", q->text_answers[0]);
    }
    
    if (strlen(q->explanation) > 0) {
        cJSON_AddStringToObject(results, "explanation", q->explanation);
    }
    
    if (session->mode == MODE_BATTLE && last_player_index >= 0) {
        cJSON_AddStringToObject(results, "lastPlayer", session->players[last_player_index].pseudo);
    }
    
    cJSON *results_array = cJSON_AddArrayToObject(results, "results");
    
    for (int i = 0; i < session->num_players; i++) {
        SessionPlayer *p = &session->players[i];
        
        cJSON *player_result = cJSON_CreateObject();
        cJSON_AddStringToObject(player_result, "pseudo", p->pseudo);
        cJSON_AddNumberToObject(player_result, "answer", p->has_answered ? p->current_answer : -1);
        
        cJSON_AddBoolToObject(player_result, "correct", p->was_correct);
        
        int points = 0;
        if (p->was_correct) {
            points = calculate_points(q->difficulty, p->response_time, session->time_limit);
        }
        cJSON_AddNumberToObject(player_result, "points", points);
        cJSON_AddNumberToObject(player_result, "totalScore", p->score);
        
        if (session->mode == MODE_BATTLE) {
            cJSON_AddNumberToObject(player_result, "responseTime", p->response_time);
            cJSON_AddNumberToObject(player_result, "lives", p->lives);
        }
        
        cJSON_AddItemToArray(results_array, player_result);
    }
    
    char *json = cJSON_PrintUnformatted(results);
    
    for (int i = 0; i < session->num_players; i++) {
        send_to_client(state, session->players[i].client_id, json);
    }
    
    free(json);
    cJSON_Delete(results);
    
    if (session->mode == MODE_BATTLE) {
        for (int i = 0; i < session->num_players; i++) {
            if (session->players[i].eliminated && session->players[i].eliminated_at == session->current_question + 1) {
                cJSON *elim = cJSON_CreateObject();
                cJSON_AddStringToObject(elim, "action", "session/player/eliminated");
                cJSON_AddStringToObject(elim, "pseudo", session->players[i].pseudo);
                
                char *elim_json = cJSON_PrintUnformatted(elim);
                for (int j = 0; j < session->num_players; j++) {
                    send_to_client(state, session->players[j].client_id, elim_json);
                }
                free(elim_json);
                cJSON_Delete(elim);
            }
        }
    }
    
    pthread_mutex_unlock(&session->mutex);
    
    int active_players = 0;
    for (int i = 0; i < session->num_players; i++) {
        if (!session->players[i].eliminated) active_players++;
    }
    
    if (session->mode == MODE_BATTLE && active_players <= 1) {
        end_session(state, session);
    } else if (session->current_question + 1 >= session->num_questions) {
        end_session(state, session);
    } else {
        #ifdef _WIN32
        Sleep(5000);
        #else
        sleep(5);
        #endif
        advance_to_next_question(state, session);
    }
}

/**
 * Advances session to the next question.
 * Increments current_question index and sends new question to all.
 * @param state Server state for sending messages
 * @param session Session to advance
 */
void advance_to_next_question(ServerState *state, Session *session) {
    pthread_mutex_lock(&session->mutex);
    session->current_question++;
    pthread_mutex_unlock(&session->mutex);
    
    send_question_to_all(state, session);
}

/**
 * Ends a game session and sends final results.
 * Sorts players for ranking, sends session/finished message to all.
 * @param state Server state for sending messages and updating clients
 * @param session Session to end
 */
void end_session(ServerState *state, Session *session) {
    pthread_mutex_lock(&session->mutex);
    
    session->status = SESSION_FINISHED;
    
    cJSON *final = cJSON_CreateObject();
    cJSON_AddStringToObject(final, "action", "session/finished");
    cJSON_AddStringToObject(final, "mode", mode_to_string(session->mode));
    
    SessionPlayer sorted_players[MAX_PLAYERS_PER_SESSION];
    memcpy(sorted_players, session->players, sizeof(SessionPlayer) * session->num_players);
    
    for (int i = 0; i < session->num_players - 1; i++) {
        for (int j = 0; j < session->num_players - i - 1; j++) {
            bool swap = false;
            
            if (session->mode == MODE_BATTLE) {
                if (sorted_players[j].lives < sorted_players[j+1].lives) {
                    swap = true;
                } else if (sorted_players[j].lives == sorted_players[j+1].lives) {
                    if (sorted_players[j].eliminated_at < sorted_players[j+1].eliminated_at) {
                        swap = true;
                    } else if (sorted_players[j].eliminated_at == sorted_players[j+1].eliminated_at) {
                        if (sorted_players[j].score < sorted_players[j+1].score) {
                            swap = true;
                        }
                    }
                }
            } else {
                if (sorted_players[j].score < sorted_players[j+1].score) {
                    swap = true;
                }
            }
            
            if (swap) {
                SessionPlayer temp = sorted_players[j];
                sorted_players[j] = sorted_players[j+1];
                sorted_players[j+1] = temp;
            }
        }
    }
    
    if (session->mode == MODE_BATTLE) {
        cJSON_AddStringToObject(final, "winner", sorted_players[0].pseudo);
    }
    
    cJSON *ranking = cJSON_AddArrayToObject(final, "ranking");
    
    for (int i = 0; i < session->num_players; i++) {
        SessionPlayer *p = &sorted_players[i];
        
        cJSON *player_rank = cJSON_CreateObject();
        cJSON_AddNumberToObject(player_rank, "rank", i + 1);
        cJSON_AddStringToObject(player_rank, "pseudo", p->pseudo);
        cJSON_AddNumberToObject(player_rank, "score", p->score);
        cJSON_AddNumberToObject(player_rank, "correctAnswers", p->correct_answers);
        
        if (session->mode == MODE_BATTLE) {
            cJSON_AddNumberToObject(player_rank, "lives", p->lives);
            if (p->eliminated) {
                cJSON_AddNumberToObject(player_rank, "eliminatedAt", p->eliminated_at);
            }
        }
        
        cJSON_AddItemToArray(ranking, player_rank);
    }
    
    char *json = cJSON_PrintUnformatted(final);
    
    for (int i = 0; i < session->num_players; i++) {
        send_to_client(state, session->players[i].client_id, json);
        
        // Update client session state
        Client *client = NULL;
        for (int c = 0; c < state->num_clients; c++) {
            if (state->clients[c].id == session->players[i].client_id) {
                client = &state->clients[c];
                break;
            }
        }
        if (client) {
            client->current_session_id = -1;
        }
    }
    
    free(json);
    cJSON_Delete(final);
    
    pthread_mutex_unlock(&session->mutex);
}

/**
 * Uses the 50/50 joker to eliminate two wrong answers.
 * Only works for QCM questions, can only be used once per session.
 * @param state Server state for getting current question
 * @param session Current game session
 * @param client_id Client using the joker
 * @param removed_answers Output array to store the two removed answer indices
 * @return 0 on success, -1 already used or answered, -2 not QCM question
 */
int use_joker_fifty(ServerState *state, Session *session, int client_id, int *removed_answers) {
    pthread_mutex_lock(&session->mutex);
    
    SessionPlayer *player = find_session_player(session, client_id);
    if (!player || player->joker_fifty_used || player->has_answered) {
        pthread_mutex_unlock(&session->mutex);
        return -1;
    }
    
    Question *q = get_current_question(state, session);
    if (!q || q->type != QUESTION_QCM) {
        pthread_mutex_unlock(&session->mutex);
        return -2;
    }
    
    player->joker_fifty_used = true;
    
    int wrong_answers[3];
    int num_wrong = 0;
    
    for (int i = 0; i < 4; i++) {
        if (i != q->correct_answer) {
            wrong_answers[num_wrong++] = i;
        }
    }
    
    shuffle_array(wrong_answers, num_wrong);
    removed_answers[0] = wrong_answers[0];
    removed_answers[1] = wrong_answers[1];
    
    pthread_mutex_unlock(&session->mutex);
    return 0;
}

/**
 * Uses the skip joker to skip the current question.
 * Player is marked as answered with neutral result, can only be used once.
 * @param state Server state (unused but kept for consistency)
 * @param session Current game session
 * @param client_id Client using the joker
 * @return 0 on success, -1 already used or answered
 */
int use_joker_skip(ServerState *state, Session *session, int client_id) {
    pthread_mutex_lock(&session->mutex);
    
    SessionPlayer *player = find_session_player(session, client_id);
    if (!player || player->joker_skip_used || player->has_answered) {
        pthread_mutex_unlock(&session->mutex);
        return -1;
    }
    
    player->joker_skip_used = true;
    player->has_answered = true;
    player->used_skip_this_question = true;
    player->current_answer = -2; // Special value for skipped
    
    pthread_mutex_unlock(&session->mutex);
    return 0;
}

/**
 * Creates JSON response with list of all waiting sessions.
 * Used for sessions/list endpoint to show available games.
 * @param state Server state containing all sessions
 * @return cJSON object with action, status, nbSessions, and sessions array
 */
cJSON* create_sessions_list_json(ServerState *state) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "sessions/list");
    cJSON_AddStringToObject(response, "statut", "200");
    cJSON_AddStringToObject(response, "message", "ok");
    
    int count = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (state->sessions[i].status == SESSION_WAITING && state->sessions[i].id > 0) {
            count++;
        }
    }
    
    cJSON_AddNumberToObject(response, "nbSessions", count);
    
    if (count > 0) {
        cJSON *sessions_array = cJSON_AddArrayToObject(response, "sessions");
        
        for (int i = 0; i < MAX_SESSIONS; i++) {
            Session *s = &state->sessions[i];
            if (s->status != SESSION_WAITING || s->id == 0) continue;
            
            cJSON *session = cJSON_CreateObject();
            cJSON_AddNumberToObject(session, "id", s->id);
            cJSON_AddStringToObject(session, "name", s->name);
            
            cJSON *theme_ids = cJSON_AddArrayToObject(session, "themeIds");
            cJSON *theme_names = cJSON_AddArrayToObject(session, "themeNames");
            
            for (int t = 0; t < s->num_themes; t++) {
                cJSON_AddItemToArray(theme_ids, cJSON_CreateNumber(s->theme_ids[t]));
                // Find theme name
                for (int th = 0; th < state->num_themes; th++) {
                    if (state->themes[th].id == s->theme_ids[t]) {
                        cJSON_AddItemToArray(theme_names, cJSON_CreateString(state->themes[th].name));
                        break;
                    }
                }
            }
            
            cJSON_AddStringToObject(session, "difficulty", difficulty_to_string(s->difficulty));
            cJSON_AddNumberToObject(session, "nbQuestions", s->num_questions);
            cJSON_AddNumberToObject(session, "timeLimit", s->time_limit);
            cJSON_AddStringToObject(session, "mode", mode_to_string(s->mode));
            cJSON_AddNumberToObject(session, "nbPlayers", s->num_players);
            cJSON_AddNumberToObject(session, "maxPlayers", s->max_players);
            cJSON_AddStringToObject(session, "status", "waiting");
            
            cJSON_AddItemToArray(sessions_array, session);
        }
    }
    
    return response;
}

/**
 * Creates JSON response for a successful session join.
 * Includes session info, player list, joker availability.
 * @param session The joined session
 * @param client_id Client who joined
 * @return cJSON object with session details and player list
 */
cJSON* create_session_join_response(Session *session, int client_id) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "session/join");
    cJSON_AddStringToObject(response, "statut", "201");
    cJSON_AddStringToObject(response, "message", "session joined");
    cJSON_AddNumberToObject(response, "sessionId", session->id);
    cJSON_AddStringToObject(response, "mode", mode_to_string(session->mode));
    cJSON_AddBoolToObject(response, "isCreator", session->creator_client_id == client_id);
    
    cJSON *players = cJSON_AddArrayToObject(response, "players");
    for (int i = 0; i < session->num_players; i++) {
        cJSON_AddItemToArray(players, cJSON_CreateString(session->players[i].pseudo));
    }
    
    if (session->mode == MODE_BATTLE) {
        cJSON_AddNumberToObject(response, "lives", session->initial_lives);
    }
    
    cJSON *jokers = cJSON_AddObjectToObject(response, "jokers");
    cJSON_AddNumberToObject(jokers, "fifty", 1);
    cJSON_AddNumberToObject(jokers, "skip", 1);
    
    return response;
}
