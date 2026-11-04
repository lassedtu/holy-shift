RAYLIB ?= /opt/homebrew
CFLAGS ?= -O2 -Wall -Wextra -I$(RAYLIB)/include
LDFLAGS ?= -L$(RAYLIB)/lib -Wl,-rpath,$(RAYLIB)/lib -lraylib

../build/level_builder: level_builder.c
	mkdir -p ../build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# The editor writes into levels/, so it must run from the project root
run: ../build/level_builder
	cd .. && ./build/level_builder

clean:
	rm -f ../build/level_builder

.PHONY: run clean
