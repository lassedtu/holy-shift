# Build of the game
# Every .c file below this folder is found by the build, a new file needs no change here
# Header changes rebuild the files that use them (the .d files)

RAYLIB ?= /opt/homebrew

BUILD_DIR := ../build
OBJ_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/holy_shift

SOURCES := $(sort $(patsubst ./%,%,$(shell find . -name '*.c')))
OBJECTS := $(SOURCES:%.c=$(OBJ_DIR)/%.o)
DEPENDENCIES := $(OBJECTS:.o=.d)

CFLAGS ?= -O2 -Wall -Wextra -std=c17
CPPFLAGS += -I. -I$(RAYLIB)/include -MMD -MP
LDFLAGS ?= -L$(RAYLIB)/lib -Wl,-rpath,$(RAYLIB)/lib
LDLIBS ?= -lraylib -lm

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# The game loads assets/ and levels/, so it must start in the project root
# Use LEVEL=<name> to select a level, the first saved level is the default
# Use ARGS to give more options, for example ARGS=--debug_fly
run: $(TARGET)
	cd .. && ./build/holy_shift $(LEVEL) $(ARGS)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

-include $(DEPENDENCIES)

.PHONY: all run clean
