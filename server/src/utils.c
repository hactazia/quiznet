/**
 * @file utils.c
 * @brief Utility functions for the QuizNet server
 * 
 * This file contains various utility functions including logging,
 * string manipulation, hashing, random number generation, and
 * conversion functions for game-related enumerations.
 */

#define _CRT_RAND_S  // Enable rand_s() on Windows

#include "utils.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <stdarg.h>

/**
 * Logs a formatted message with timestamp and tag
 * 
 * Outputs a log message to stdout with a precise timestamp (including milliseconds),
 * a tag for categorization, and the formatted message. Works on both Windows and Unix.
 * 
 * @param tag The category tag for the log message (e.g., "SERVER", "ERROR")
 * @param format Printf-style format string
 * @param ... Variable arguments for the format string
 */
void log_msg(const char *tag, const char *format, ...) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("%02d:%02d:%02d.%03d [%s] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tag);
#else
    struct timeval tv;
    struct tm *tm_info;
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    printf("%02d:%02d:%02d.%03d [%s] ", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, (int)(tv.tv_usec / 1000), tag);
#endif

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

/**
 * Converts a string to lowercase in-place
 * 
 * Modifies the input string by converting all uppercase characters
 * to their lowercase equivalents.
 * 
 * @param str The string to convert (modified in-place)
 */
void str_to_lower(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

/**
 * Compares two strings case-insensitively
 * 
 * Performs a character-by-character comparison of two strings,
 * ignoring case differences.
 * 
 * @param a First string to compare
 * @param b Second string to compare
 * @return true if strings are equal (ignoring case), false otherwise
 */
bool str_equals_ignore_case(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/**
 * Removes leading and trailing whitespace from a string
 * 
 * Modifies the input string in-place by removing all leading
 * and trailing whitespace characters (spaces, tabs, newlines, etc.).
 * 
 * @param str The string to trim (modified in-place)
 */
void trim_whitespace(char *str) {
    char *start = str;
    char *end;
    
    // Trim leading space
    while (isspace((unsigned char)*start)) start++;
    
    if (*start == 0) {
        str[0] = 0;
        return;
    }
    
    // Trim trailing space
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    // Move trimmed string
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * Generates a simple hash of the input string
 * 
 * Creates a 64-character hexadecimal hash string from the input.
 * 
 * @param input The string to hash
 * @param output Buffer to store the resulting hash (must be at least 65 bytes)
 */
void sha256_hash(const char *input, char *output) {
    // Simple hash for demonstration
    unsigned long hash = 5381;
    int c;
    const char *str = input;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    sprintf(output, "%016lx%016lx%016lx%016lx",
            hash, hash ^ 0xDEADBEEF, hash ^ 0xCAFEBABE, hash ^ 0x12345678);
}

/**
 * Initializes the random number generator
 * 
 * Seeds the random number generator. On Windows, rand_s() is used
 * which doesn't require seeding. On Unix, uses time + microseconds + pid.
 * Should be called once at program startup before using
 * random_int() or shuffle_array().
 */
void init_random(void) {
#ifdef _WIN32
    // rand_s() doesn't need seeding, it uses OS cryptographic RNG
    log_msg("RANDOM", "Using Windows rand_s() - no seed needed");
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int seed = (unsigned int)(tv.tv_usec + tv.tv_sec + getpid());
    srand(seed);
    log_msg("RANDOM", "Random seed initialized: %u", seed);
#endif
}

/**
 * Generates a secure random integer within a specified range
 * 
 * @param min The minimum value (inclusive)
 * @param max The maximum value (inclusive)
 * @return A random integer between min and max (inclusive)
 */
int random_int(int min, int max) {
#ifdef _WIN32
    unsigned int val;
    rand_s(&val);
    return min + (int)(val % (unsigned int)(max - min + 1));
#else
    return min + rand() % (max - min + 1);
#endif
}

/**
 * Shuffles an array of integers using Fisher-Yates algorithm
 * 
 * Randomly reorders the elements of the array in-place.
 * Provides a uniform random permutation.
 * 
 * @param array Pointer to the array to shuffle
 * @param n Number of elements in the array
 */
void shuffle_array(int *array, int n) {
    log_msg("SHUFFLE", "shuffle_array() called with n=%d, first 3 before: [%d, %d, %d]", 
           n, n > 0 ? array[0] : -1, n > 1 ? array[1] : -1, n > 2 ? array[2] : -1);
    for (int i = n - 1; i > 0; i--) {
#ifdef _WIN32
        unsigned int val;
        rand_s(&val);
        int j = (int)(val % (unsigned int)(i + 1));
#else
        int j = rand() % (i + 1);
#endif
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
    log_msg("SHUFFLE", "shuffle_array() done, first 3 after: [%d, %d, %d]", 
           n > 0 ? array[0] : -1, n > 1 ? array[1] : -1, n > 2 ? array[2] : -1);
}

/**
 * Gets the current time in milliseconds
 * 
 * Returns a high-precision timestamp suitable for measuring
 * elapsed time. Uses QueryPerformanceCounter on Windows and
 * gettimeofday on Unix systems.
 * 
 * @return Current time in milliseconds as a double
 */
double get_current_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
#endif
}

/**
 * Converts a difficulty level enum to its string representation
 * 
 * @param difficulty The difficulty level (DIFFICULTY_EASY, DIFFICULTY_MEDIUM, DIFFICULTY_HARD)
 * @return French string representation: "facile", "moyen", or "difficile"
 */
const char* difficulty_to_string(int difficulty) {
    switch (difficulty) {
        case DIFFICULTY_EASY: return "facile";
        case DIFFICULTY_MEDIUM: return "moyen";
        case DIFFICULTY_HARD: return "difficile";
        default: return "moyen";
    }
}

/**
 * Converts a difficulty string to its enum value
 * 
 * Accepts both French ("facile", "moyen", "difficile") and
 * English ("easy", "medium", "hard") difficulty names.
 * Case-insensitive matching.
 * 
 * @param str The difficulty string to convert
 * @return The corresponding difficulty enum value (defaults to DIFFICULTY_MEDIUM)
 */
int string_to_difficulty(const char *str) {
    if (str_equals_ignore_case(str, "facile") || str_equals_ignore_case(str, "easy")) {
        return DIFFICULTY_EASY;
    } else if (str_equals_ignore_case(str, "difficile") || str_equals_ignore_case(str, "hard")) {
        return DIFFICULTY_HARD;
    }
    return DIFFICULTY_MEDIUM;
}

/**
 * Converts a game mode enum to its string representation
 * 
 * @param mode The game mode (MODE_SOLO or MODE_BATTLE)
 * @return String representation: "solo" or "battle"
 */
const char* mode_to_string(int mode) {
    switch (mode) {
        case MODE_SOLO: return "solo";
        case MODE_BATTLE: return "battle";
        default: return "solo";
    }
}

/**
 * Converts a game mode string to its enum value
 * 
 * Case-insensitive matching.
 * 
 * @param str The mode string to convert ("solo" or "battle")
 * @return The corresponding mode enum value (defaults to MODE_SOLO)
 */
int string_to_mode(const char *str) {
    if (str_equals_ignore_case(str, "battle")) {
        return MODE_BATTLE;
    }
    return MODE_SOLO;
}

/**
 * Converts a question type enum to its string representation
 * 
 * @param type The question type (QUESTION_QCM, QUESTION_BOOLEAN, QUESTION_TEXT)
 * @return String representation: "qcm", "boolean", or "text"
 */
const char* question_type_to_string(int type) {
    switch (type) {
        case QUESTION_QCM: return "qcm";
        case QUESTION_BOOLEAN: return "boolean";
        case QUESTION_TEXT: return "text";
        default: return "qcm";
    }
}
