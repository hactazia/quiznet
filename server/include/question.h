#ifndef QUESTION_H
#define QUESTION_H

#include "types.h"
#include "cJSON.h"

int load_questions(ServerState *state, const char *filename);
int select_questions_for_session(ServerState *state, Session *session);
bool check_answer(Question *q, int answer_index, const char *text_answer, bool bool_answer);
int calculate_points(Difficulty difficulty, double response_time, int time_limit);
cJSON* create_themes_json(ServerState *state);

#endif // QUESTION_H
