#ifndef SESSION_H
#define SESSION_H

#include "cJSON.h"
#include "types.h"

Session* create_session(ServerState* state, const char* name, int* theme_ids,
                        int num_themes, Difficulty difficulty,
                        int num_questions, int time_limit, GameMode mode,
                        int initial_lives, int max_players,
                        int creator_client_id);
Session* find_session(ServerState* state, int session_id);
int join_session(ServerState* state, Session* session, int client_id,
                 const char* pseudo);
int leave_session(ServerState* state, Session* session, int client_id);
int start_session(ServerState* state, Session* session);
SessionPlayer* find_session_player(Session* session, int client_id);
SessionPlayer* find_session_player_by_pseudo(Session* session,
                                             const char* pseudo);
void send_question_to_all(ServerState* state, Session* session);
void process_answer(ServerState* state, Session* session, int client_id,
                    int answer_index, const char* text_answer, bool bool_answer,
                    double response_time);
void check_question_timeout(ServerState* state, Session* session);
void send_question_results(ServerState* state, Session* session);
void advance_to_next_question(ServerState* state, Session* session);
void end_session(ServerState* state, Session* session);
int use_joker_fifty(ServerState* state, Session* session, int client_id,
                    int* removed_answers);
int use_joker_skip(ServerState* state, Session* session, int client_id);
cJSON* create_sessions_list_json(ServerState* state);
cJSON* create_session_join_response(Session* session, int client_id);

#endif  // SESSION_H
