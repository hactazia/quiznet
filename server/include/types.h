/**
 * @file types.h
 * @brief Core type definitions for the QuizNet server
 * 
 * This header defines all the fundamental data structures and constants
 * used throughout the QuizNet multiplayer quiz game server. It includes
 * configuration limits, enumerations for game states, and structures
 * for players, sessions, questions, and server state management.
 */

#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/** @defgroup config Server Configuration Limits
 *  Maximum values for various server resources
 *  @{
 */
#define MAX_CLIENTS 100              /**< Maximum simultaneous client connections */
#define MAX_SESSIONS 20              /**< Maximum concurrent game sessions */
#define MAX_PLAYERS_PER_SESSION 10   /**< Maximum players in a single session */
#define MAX_QUESTIONS 200            /**< Maximum questions in the database */
#define MAX_THEMES 20                /**< Maximum number of question themes/categories */
#define MAX_PSEUDO_LEN 32            /**< Maximum length of player username */
#define MAX_PASSWORD_LEN 64          /**< Maximum length of player password */
#define MAX_MESSAGE_LEN 8192         /**< Maximum length of protocol messages */
#define MAX_QUESTION_TEXT 512        /**< Maximum length of question text */
#define MAX_ANSWER_TEXT 128          /**< Maximum length of an answer option */
#define MAX_THEME_NAME 64            /**< Maximum length of a theme name */
/** @} */

/** @defgroup network Network Configuration
 *  Default ports and server identification
 *  @{
 */
#define UDP_PORT 5555                /**< UDP port for server discovery broadcasts */
#define DEFAULT_TCP_PORT 5556        /**< Default TCP port for game connections */
/** @} */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Types of questions supported by the quiz system
 */
typedef enum {
    QUESTION_QCM,      /**< Multiple choice question (4 options) */
    QUESTION_BOOLEAN,  /**< True/False question */
    QUESTION_TEXT      /**< Free text answer question */
} QuestionType;

/**
 * @brief Difficulty levels for questions
 * 
 * Affects scoring multipliers and question selection.
 */
typedef enum {
    DIFFICULTY_EASY,   /**< Easy questions - lower point value */
    DIFFICULTY_MEDIUM, /**< Medium difficulty - standard points */
    DIFFICULTY_HARD    /**< Hard questions - higher point value */
} Difficulty;

/**
 * @brief Game modes available for quiz sessions
 */
typedef enum {
    MODE_SOLO,   /**< Single player mode - play at your own pace */
    MODE_BATTLE  /**< Multiplayer battle - compete with lives system */
} GameMode;

/**
 * @brief Current status of a game session
 */
typedef enum {
    SESSION_WAITING,  /**< Waiting for players to join before starting */
    SESSION_PLAYING,  /**< Game is in progress */
    SESSION_FINISHED  /**< Game has ended */
} SessionStatus;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Represents a question category/theme
 * 
 * Themes are used to categorize questions and allow players
 * to select specific topics for their quiz sessions.
 */
typedef struct {
    int id;                    /**< Unique theme identifier */
    char name[MAX_THEME_NAME]; /**< Display name of the theme */
} Theme;

/**
 * @brief Represents a quiz question with all its metadata
 * 
 * Supports multiple question types (QCM, boolean, text) and can
 * belong to multiple themes. Includes explanation for learning.
 */
typedef struct {
    int id;                                /**< Unique question identifier */
    int theme_ids[MAX_THEMES];             /**< Array of theme IDs this question belongs to */
    int num_themes;                        /**< Number of themes assigned to this question */
    Difficulty difficulty;                 /**< Difficulty level of the question */
    QuestionType type;                     /**< Type of question (QCM, boolean, text) */
    char question[MAX_QUESTION_TEXT];      /**< The question text */
    char answers[4][MAX_ANSWER_TEXT];      /**< Answer options (used for QCM type) */
    int correct_answer;                    /**< Correct answer index (0-3 for QCM, 0/1 for boolean) */
    char text_answers[4][MAX_ANSWER_TEXT]; /**< Accepted text answers (for text type questions) */
    int num_text_answers;                  /**< Number of accepted text answers */
    char explanation[MAX_QUESTION_TEXT];   /**< Explanation shown after answering */
} Question;

/**
 * @brief Player state within a game session
 * 
 * Tracks all player-specific data during a game including score,
 * lives (for battle mode), joker usage, and current question state.
 */
typedef struct {
    int client_id;               /**< Reference to the connected client */
    char pseudo[MAX_PSEUDO_LEN]; /**< Player's display name */
    int score;                   /**< Current total score */
    int lives;                   /**< Remaining lives (battle mode) */
    int correct_answers;         /**< Count of correct answers given */
    bool has_answered;           /**< Whether player answered current question */
    bool was_correct;            /**< Whether current answer was correct */
    int current_answer;          /**< Player's answer for current question */
    double response_time;        /**< Time taken to answer (in seconds) */
    bool eliminated;             /**< Whether player is eliminated (battle mode) */
    int eliminated_at;           /**< Question number when eliminated */
    bool joker_fifty_used;       /**< Whether 50/50 joker has been used */
    bool joker_skip_used;        /**< Whether skip joker has been used */
    bool used_skip_this_question;/**< Whether skip was used on current question */
} SessionPlayer;

/**
 * @brief Represents a game session (lobby + active game)
 * 
 * A session is created by a player, configured with themes/difficulty,
 * and can host multiple players. Manages the full game lifecycle from
 * waiting room to game completion.
 */
typedef struct {
    int id;                        /**< Unique session identifier */
    char name[64];                 /**< Display name of the session */
    int theme_ids[MAX_THEMES];     /**< Selected theme IDs for this session */
    int num_themes;                /**< Number of themes selected */
    Difficulty difficulty;         /**< Difficulty level for questions */
    int num_questions;             /**< Total questions in this game */
    int time_limit;                /**< Time limit per question (seconds) */
    GameMode mode;                 /**< Game mode (solo or battle) */
    int initial_lives;             /**< Starting lives for battle mode */
    int max_players;               /**< Maximum players allowed */
    SessionStatus status;          /**< Current session status */
    
    SessionPlayer players[MAX_PLAYERS_PER_SESSION]; /**< Array of players in session */
    int num_players;               /**< Current number of players */
    int creator_client_id;         /**< Client ID of session creator (host) */
    
    int question_ids[50];          /**< IDs of questions selected for this game */
    int current_question;          /**< Index of current question (0-based) */
    time_t question_start_time;    /**< Timestamp when current question started */
    
    pthread_mutex_t mutex;         /**< Mutex for thread-safe session access */
} Session;

/**
 * @brief Persistent player account for authentication
 * 
 * Stores player credentials for login/registration.
 * Password is stored as a hashed value, never plaintext.
 */
typedef struct {
    int id;                        /**< Unique account identifier */
    char pseudo[MAX_PSEUDO_LEN];   /**< Username/display name */
    char password_hash[65];        /**< SHA256 hash of password (64 hex chars + null) */
    bool logged_in;                /**< Whether account is currently logged in */
} PlayerAccount;

/**
 * @brief Represents an active client connection
 * 
 * Tracks connection state, authentication status, and current
 * game session for each connected client.
 */
typedef struct {
    int socket;                    /**< TCP socket file descriptor */
    int id;                        /**< Unique client identifier */
    bool connected;                /**< Whether client is currently connected */
    bool authenticated;            /**< Whether client has logged in */
    char pseudo[MAX_PSEUDO_LEN];   /**< Player's username (if authenticated) */
    int current_session_id;        /**< ID of session player is in (-1 if none) */
    pthread_t thread;              /**< Thread handling this client's messages */
    char ip[16];                   /**< Client's IP address (IPv4) */
    int port;                      /**< Client's port number */
} Client;

/**
 * @brief Global server state containing all runtime data
 * 
 * This is the central data structure for the server, containing
 * all clients, sessions, questions, themes, and accounts.
 * Access to shared resources is protected by mutexes.
 */
typedef struct {
    /* Server identity */
    char server_name[64];          /**< Server name for discovery */
    
    /* Network sockets */
    int tcp_socket;                /**< Main TCP listening socket */
    int udp_socket;                /**< UDP socket for discovery broadcasts */
    int tcp_port;                  /**< TCP port the server is listening on */
    int udp_port;                  /**< UDP port for discovery */
    int next_client_id;            /**< Next ID to assign to a new client */
    pthread_t udp_thread;          /**< Thread handle for UDP discovery handler */
    
    /* Client management */
    Client clients[MAX_CLIENTS];   /**< Array of all client connections */
    int num_clients;               /**< Current number of connected clients */
    pthread_mutex_t clients_mutex; /**< Mutex for clients array access */
    
    /* Session management */
    Session sessions[MAX_SESSIONS];/**< Array of all game sessions */
    int num_sessions;              /**< Current number of active sessions */
    int next_session_id;           /**< Next ID to assign to a new session */
    pthread_mutex_t sessions_mutex;/**< Mutex for sessions array access */
    
    /* Question database */
    Question questions[MAX_QUESTIONS]; /**< Array of all loaded questions */
    int num_questions;             /**< Total number of questions loaded */
    
    /* Theme database */
    Theme themes[MAX_THEMES];      /**< Array of all available themes */
    int num_themes;                /**< Total number of themes */
    
    /* Account management */
    PlayerAccount accounts[MAX_CLIENTS]; /**< Array of registered accounts */
    int num_accounts;              /**< Total number of registered accounts */
    pthread_mutex_t accounts_mutex;/**< Mutex for accounts array access */
    pthread_mutex_t players_mutex; /**< Mutex for player-related operations */
    int num_players;               /**< Current number of active players */
    
    bool running;                  /**< Server running flag (false to shutdown) */
} ServerState;

#endif // TYPES_H
