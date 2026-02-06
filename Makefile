# ==========================================
# memscope - Memory Behavior Forensic Engine
# ==========================================

# --- CONFIGURATION ---
CC      := gcc
# -MMD -MP: Generates .d dependency files automatically
# -g: Adds debug symbols (crucial for valgrind/gdb)
CFLAGS  := -Wall -Wextra -Werror -pedantic -std=c11 -O2 -g -MMD -MP -Iinc

# Paths
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
TARGET  := $(BIN_DIR)/memscope

# Sources (Find all .c files recursively)
SRCS    := $(shell find $(SRC_DIR) -name '*.c')
# Objects (Map src/%.c -> obj/%.o)
OBJS    := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
# Dependencies
DEPS    := $(OBJS:.o=.d)

# --- KEYWORDS ---
.PHONY: all compile run clean directories test

# Default target
all: directories $(TARGET)

# Alias for 'all'
compile: all

# Compile and Run
run: all
	@echo "  [RUN] $(TARGET)"
	@./$(TARGET)

# --- BUILD RULES ---

# Link the final binary
# The "| directories" part ensures folders exist before linking
$(TARGET): $(OBJS) | directories
	@echo "  [LD]  $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files to objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Create directories (Order-only prerequisite)
directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

# Clean up
clean:
	@echo "  [RM]  $(OBJ_DIR) $(BIN_DIR)"
	@rm -rf $(OBJ_DIR) $(BIN_DIR)

# Include dependencies
-include $(DEPS)