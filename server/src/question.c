#include "question.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUESTIONS_FILE "data/questions.dat"

/**
 * Helper function to extract the next semicolon-delimited field from a string.
 * Handles empty fields correctly (unlike strtok).
 * @param ptr Pointer to current position in string (updated after call)
 * @return Pointer to the extracted field, or NULL if end of string
 */
static char* get_next_field(char **ptr) {
    if (*ptr == NULL || **ptr == '\0') return NULL;
    
    char *start = *ptr;
    char *semicolon = strchr(start, ';');
    
    if (semicolon) {
        *semicolon = '\0';
        *ptr = semicolon + 1;
    } else {
        *ptr = NULL;
    }
    
    return start;
}

/**
 * Finds an existing theme by name or creates a new one.
 * Used during question loading to auto-detect themes.
 * @param state Server state containing themes array
 * @param theme_name Name of the theme to find or create
 * @return Theme ID on success, -1 if max themes reached
 */
static int get_or_create_theme(ServerState *state, const char *theme_name) {

    for (int i = 0; i < state->num_themes; i++) {
        if (strcmp(state->themes[i].name, theme_name) == 0) {
            return state->themes[i].id;
        }
    }
    
    if (state->num_themes < MAX_THEMES) {
        int new_id = state->num_themes;
        state->themes[new_id].id = new_id;
        strncpy(state->themes[new_id].name, theme_name, 63);
        state->num_themes++;
        log_msg("QUESTION", "Created new theme: id=%d, name='%s'", new_id, theme_name);
        return new_id;
    }
    
    log_msg("QUESTION", "WARNING - Max themes reached, cannot create '%s'", theme_name);
    return -1;
}

/**
 * Loads questions from a data file into server state.
 * Parses format: theme;difficulty;type;question;answers;correct;explanation
 * @param state Server state to populate with questions
 * @param filename Path to questions file, or NULL for default "data/questions.dat"
 * @return Number of questions loaded, -1 on file error
 */
int load_questions(ServerState *state, const char *filename) {
    const char *file_to_load = filename ? filename : QUESTIONS_FILE;
    log_msg("QUESTION", "load_questions() - loading from '%s'", file_to_load);
    
    FILE *file = fopen(file_to_load, "r");
    if (!file) {
        log_msg("QUESTION", "ERROR - Cannot open questions file '%s'", file_to_load);
        return -1;
    }
    
    char line[2048];
    state->num_questions = 0;
    int next_question_id = 1; 
    int line_num = 0;
    
    while (fgets(line, sizeof(line), file) && state->num_questions < MAX_QUESTIONS) {
        line_num++;
        trim_whitespace(line);
        if (strlen(line) == 0 || line[0] == '#') continue;
        
        log_msg("QUESTION", "Parsing line %d: %.50s...", line_num, line);
        
        Question *q = &state->questions[state->num_questions];
        memset(q, 0, sizeof(Question));
        q->id = next_question_id++;
        char line_copy[2048];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        char *ptr = line_copy;
        
        // Parse: theme;difficulty;type;question;answers;correct;explanation
        char *field;
        
        // themes
        field = get_next_field(&ptr);
        if (!field) continue;
        if (strlen(field) > 0) {
            char themes_str[256];
            strncpy(themes_str, field, 255);
            char *theme_ptr = themes_str;
            char *comma;
            
            while ((comma = strchr(theme_ptr, ',')) != NULL && q->num_themes < MAX_THEMES) {
                *comma = '\0';

                while (*theme_ptr == ' ') theme_ptr++;
                char *end = theme_ptr + strlen(theme_ptr) - 1;
                while (end > theme_ptr && *end == ' ') *end-- = '\0';
                
                int theme_id = get_or_create_theme(state, theme_ptr);
                if (theme_id >= 0) {
                    q->theme_ids[q->num_themes++] = theme_id;
                }
                theme_ptr = comma + 1;
            }

            if (*theme_ptr && q->num_themes < MAX_THEMES) {
                while (*theme_ptr == ' ') theme_ptr++;
                char *end = theme_ptr + strlen(theme_ptr) - 1;
                while (end > theme_ptr && *end == ' ') *end-- = '\0';
                
                int theme_id = get_or_create_theme(state, theme_ptr);
                if (theme_id >= 0) {
                    q->theme_ids[q->num_themes++] = theme_id;
                }
            }
        }
        
        // difficulty
        field = get_next_field(&ptr);
        if (!field) continue;
        q->difficulty = string_to_difficulty(field);
        
        // type
        field = get_next_field(&ptr);
        if (!field) continue;
        if (strcmp(field, "qcm") == 0) q->type = QUESTION_QCM;
        else if (strcmp(field, "boolean") == 0) q->type = QUESTION_BOOLEAN;
        else q->type = QUESTION_TEXT;
        
        // Field 5: question text
        field = get_next_field(&ptr);
        if (!field) continue;
        strncpy(q->question, field, MAX_QUESTION_TEXT - 1);
        
        // answers
        field = get_next_field(&ptr);
        if (!field) continue;
        if (q->type == QUESTION_QCM && strlen(field) > 0) {
            char *ans_ptr = field;
            char *comma;
            int ans_idx = 0;
            while ((comma = strchr(ans_ptr, ',')) != NULL && ans_idx < 4) {
                *comma = '\0';
                strncpy(q->answers[ans_idx++], ans_ptr, MAX_ANSWER_TEXT - 1);
                ans_ptr = comma + 1;
            }
            if (*ans_ptr && ans_idx < 4) {
                strncpy(q->answers[ans_idx], ans_ptr, MAX_ANSWER_TEXT - 1);
            }
        }
        
        // correct answers
        field = get_next_field(&ptr);
        if (!field) continue;
        if (q->type == QUESTION_TEXT && strlen(field) > 0) {
            char *cor_ptr = field;
            char *comma;
            while ((comma = strchr(cor_ptr, ',')) != NULL && q->num_text_answers < 4) {
                *comma = '\0';
                strncpy(q->text_answers[q->num_text_answers++], cor_ptr, MAX_ANSWER_TEXT - 1);
                cor_ptr = comma + 1;
            }
            if (*cor_ptr && q->num_text_answers < 4) {
                strncpy(q->text_answers[q->num_text_answers++], cor_ptr, MAX_ANSWER_TEXT - 1);
            }
        } else {
            q->correct_answer = atoi(field);
        }
        
        // explanation
        field = get_next_field(&ptr);
        if (field && strlen(field) > 0) {
            strncpy(q->explanation, field, MAX_QUESTION_TEXT - 1);
        }
        
        state->num_questions++;
    }
    
    fclose(file);
    log_msg("QUESTION", "Loaded %d questions from %s", state->num_questions, file_to_load);
    log_msg("QUESTION", "Detected %d themes:", state->num_themes);
    for (int i = 0; i < state->num_themes; i++) {
        log_msg("QUESTION", "  [%d] %s", state->themes[i].id, state->themes[i].name);
    }
    return state->num_questions;
}

/**
 * Selects random questions for a game session based on criteria.
 * Filters by difficulty and themes, then shuffles and picks required number.
 * @param state Server state containing all questions
 * @param session Session with theme IDs, difficulty, and num_questions set
 * @return Number of questions selected, -1 if not enough matching questions
 */
int select_questions_for_session(ServerState *state, Session *session) {
    log_msg("QUESTION", "select_questions_for_session() - need %d questions, difficulty=%d",
           session->num_questions, session->difficulty);
    
    int matching[MAX_QUESTIONS];
    int num_matching = 0;
    
    for (int i = 0; i < state->num_questions; i++) {
        Question *q = &state->questions[i];
        
        if (q->difficulty != session->difficulty) continue;
        
        bool theme_match = false;
        for (int t = 0; t < session->num_themes && !theme_match; t++) {
            for (int qt = 0; qt < q->num_themes && !theme_match; qt++) {
                if (q->theme_ids[qt] == session->theme_ids[t]) {
                    theme_match = true;
                }
            }
        }
        
        if (theme_match) {
            matching[num_matching++] = i;
        }
    }
    
    if (num_matching < session->num_questions) {
        log_msg("QUESTION", "select_questions_for_session() FAILED - only %d matching (need %d)",
               num_matching, session->num_questions);
        return -1;
    }
    
    log_msg("QUESTION", "Found %d matching questions, selecting %d", 
           num_matching, session->num_questions);
    
    shuffle_array(matching, num_matching);
    
    for (int i = 0; i < session->num_questions; i++) {
        session->question_ids[i] = state->questions[matching[i]].id;
        log_msg("QUESTION", "  Selected question id=%d", session->question_ids[i]);
    }
    
    return session->num_questions;
}

/**
 * Validates a player's answer against the correct answer.
 * Handles QCM (index), boolean, and text (case-insensitive) question types.
 * @param q The question being answered
 * @param answer_index For QCM: the selected answer index (0-3)
 * @param text_answer For TEXT: the player's text response
 * @param bool_answer For BOOLEAN: the player's true/false response
 * @return true if answer is correct, false otherwise
 */
bool check_answer(Question *q, int answer_index, const char *text_answer, bool bool_answer) {
    bool correct = false;
    
    switch (q->type) {
        case QUESTION_QCM:
            correct = (answer_index == q->correct_answer);
            log_msg("QUESTION", "check_answer(QCM) - given=%d, expected=%d, correct=%s",
                   answer_index, q->correct_answer, correct ? "YES" : "NO");
            return correct;
            
        case QUESTION_BOOLEAN:
            correct = (bool_answer == (q->correct_answer == 1));
            log_msg("QUESTION", "check_answer(BOOL) - given=%s, expected=%d, correct=%s",
                   bool_answer ? "true" : "false", q->correct_answer, correct ? "YES" : "NO");
            return correct;
            
        case QUESTION_TEXT:
            for (int i = 0; i < q->num_text_answers; i++) {
                if (str_equals(text_answer, q->text_answers[i])) {
                    log_msg("QUESTION", "check_answer(TEXT) - given='%s', matched='%s', correct=YES",
                           text_answer, q->text_answers[i]);
                    return true;
                }
            }
            log_msg("QUESTION", "check_answer(TEXT) - given='%s', correct=NO", text_answer);
            return false;
            
        default:
            log_msg("QUESTION", "check_answer() - unknown question type");
            return false;
    }
}

/**
 * Calculates points awarded for a correct answer.
 * Base points depend on difficulty, bonus points for fast response.
 * @param difficulty Question difficulty level
 * @param response_time Time taken to answer in seconds
 * @param time_limit Maximum allowed time in seconds
 * @return Points to award (base + bonus if answered in first half of time)
 */
int calculate_points(Difficulty difficulty, double response_time, int time_limit) {
    int base_points = 0;
    int bonus_points = 0;
    
    switch (difficulty) {
        case DIFFICULTY_EASY:
            base_points = 5;
            bonus_points = 1;
            break;
        case DIFFICULTY_MEDIUM:
            base_points = 10;
            bonus_points = 3;
            break;
        case DIFFICULTY_HARD:
            base_points = 15;
            bonus_points = 6;
            break;
    }
    
    if (response_time <= (double)time_limit / 2.0) {
        return base_points + bonus_points;
    }
    
    return base_points;
}

/**
 * Creates JSON response with list of all available themes.
 * Used for themes/list endpoint.
 * @param state Server state containing themes array
 * @return cJSON object with action, status, nbThemes, and themes array
 */
cJSON* create_themes_json(ServerState *state) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "themes/list");
    cJSON_AddStringToObject(response, "statut", "200");
    cJSON_AddStringToObject(response, "message", "ok");
    cJSON_AddNumberToObject(response, "nbThemes", state->num_themes);
    
    cJSON *themes_array = cJSON_AddArrayToObject(response, "themes");
    for (int i = 0; i < state->num_themes; i++) {
        cJSON *theme = cJSON_CreateObject();
        cJSON_AddNumberToObject(theme, "id", state->themes[i].id);
        cJSON_AddStringToObject(theme, "name", state->themes[i].name);
        cJSON_AddItemToArray(themes_array, theme);
    }
    
    return response;
}