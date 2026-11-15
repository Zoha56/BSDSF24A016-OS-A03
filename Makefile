CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/shell.c $(SRC_DIR)/execute.c
OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/shell.o $(OBJ_DIR)/execute.o
TARGET = $(BIN_DIR)/myshell

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean
