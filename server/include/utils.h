#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdarg.h>

void log_msg(const char *tag, const char *format, ...);
void str_to_lower(char *str);
bool str_equals_ignore_case(const char *a, const char *b);
void trim_whitespace(char *str);
void sha256_hash(const char *input, char *output);
void init_random(void);
int random_int(int min, int max);
void shuffle_array(int *array, int n);
double get_current_time_ms(void);
const char* difficulty_to_string(int difficulty);
int string_to_difficulty(const char *str);
const char* mode_to_string(int mode);
int string_to_mode(const char *str);
const char* question_type_to_string(int type);

#endif // UTILS_H
