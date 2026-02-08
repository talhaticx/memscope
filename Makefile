# ==========================================
# memscope - Universal Makefile
# ==========================================

# --- CONFIGURATION ---
CC      := gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c11 -O2 -g -MMD -MP -Iinc
LDFLAGS := -lncurses
# Paths
SRC_DIR := src
TEST_DIR:= tests
OBJ_DIR := obj
BIN_DIR := bin

# Main Output Binary
TARGET  := $(BIN_DIR)/memscope

# --- SOURCE MANAGEMENT ---
# 1. Find all .c files in src/
ALL_SRCS    := $(shell find $(SRC_DIR) -name '*.c')

# 2. Identify the Main App Sources (Everything)
APP_OBJS    := $(ALL_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# 3. Identify Library Sources (Everything EXCEPT main.c)
#    We need this so we don't have two 'main()' functions when compiling tests.
LIB_SRCS    := $(filter-out $(SRC_DIR)/main.c, $(ALL_SRCS))
LIB_OBJS    := $(LIB_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# --- KEYWORDS ---
.PHONY: all compile run clean directories list_tests

# Default: Build the main app
all: directories $(TARGET)

# Compile and Run the Main App
run: all
	@echo "  [RUN] $(TARGET)"
	@./$(TARGET)

# --- BUILD RULES (APP) ---

# Link the Main Application
$(TARGET): $(APP_OBJS) | directories
	@echo "  [LD]  $@"
	@$(CC) $(APP_OBJS) -o $@ $(LDFLAGS)

# Compile Generic Source Files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# --- BUILD RULES (TESTS) ---

# Magic Rule: 'make test_stat' compiles tests/test_stat.c + src files (no main)
# Usage: make test_stat
test_%: $(TEST_DIR)/test_%.c $(LIB_OBJS) | directories
	@echo "  [TEST] Building $@"
	@$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $(BIN_DIR)/$@
	@echo "  [EXEC] Running $(BIN_DIR)/$@"
	@./$(BIN_DIR)/$@

# --- UTILS ---

directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

clean:
	@echo "  [RM]  $(OBJ_DIR) $(BIN_DIR)"
	@rm -rf $(OBJ_DIR) $(BIN_DIR)

# Helper to see what tests are available
list_tests:
	@ls $(TEST_DIR) | grep "test_" | sed 's/test_//g' | sed 's/.c//g'