CFLAGS = -Wall -Wextra -g -I./include -I./lib

PLATFORM ?= auto

ifeq ($(OS),Windows_NT)
    RM = del /Q
    RMDIR = rmdir /S /Q
    MKDIR = mkdir
else
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
endif

ifeq ($(PLATFORM),auto)
ifeq ($(OS),Windows_NT)
    PLATFORM = windows
else
    PLATFORM = linux
endif
endif

ifeq ($(PLATFORM),windows)
    ifeq ($(OS),Windows_NT)
        CC = gcc
    else
        CC = x86_64-w64-mingw32-gcc
    endif
    LDFLAGS = -lws2_32
    TARGET = quiznet_server.exe
    OBJ_DIR = obj_windows
else
    CC = gcc
    LDFLAGS = -lpthread -lm
    TARGET = quiznet_server
    OBJ_DIR = obj_linux
endif

SRC_DIR = src
LIB_DIR = lib
HANDLERS_DIR = $(SRC_DIR)/handlers

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/server.c $(SRC_DIR)/protocol.c \
       $(SRC_DIR)/discover.c $(SRC_DIR)/session.c $(SRC_DIR)/player.c \
       $(SRC_DIR)/question.c $(SRC_DIR)/utils.c $(LIB_DIR)/cJSON.c \
       $(HANDLERS_DIR)/common.c $(HANDLERS_DIR)/player.c $(HANDLERS_DIR)/session.c \
       $(HANDLERS_DIR)/game.c $(HANDLERS_DIR)/joker.c

OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/server.o $(OBJ_DIR)/protocol.o \
       $(OBJ_DIR)/discover.o $(OBJ_DIR)/session.o $(OBJ_DIR)/player.o \
       $(OBJ_DIR)/question.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/cJSON.o \
       $(OBJ_DIR)/handlers_common.o $(OBJ_DIR)/handlers_player.o $(OBJ_DIR)/handlers_session.o \
       $(OBJ_DIR)/handlers_game.o $(OBJ_DIR)/handlers_joker.o

all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/cJSON.o: $(LIB_DIR)/cJSON.c
	$(CC) $(CFLAGS) -c $< -o $@

# Handler files
$(OBJ_DIR)/handlers_common.o: $(HANDLERS_DIR)/common.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/handlers_player.o: $(HANDLERS_DIR)/player.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/handlers_session.o: $(HANDLERS_DIR)/session.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/handlers_game.o: $(HANDLERS_DIR)/game.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/handlers_joker.o: $(HANDLERS_DIR)/joker.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	if exist $(OBJ_DIR) $(RMDIR) $(OBJ_DIR)
	if exist $(TARGET) $(RM) $(TARGET)
else
	$(RMDIR) $(OBJ_DIR)
	$(RM) $(TARGET)
endif

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
