RAYLIB ?= /opt/homebrew
CFLAGS ?= -O2 -Wall -Wextra -I$(RAYLIB)/include
LDFLAGS ?= -L$(RAYLIB)/lib -Wl,-rpath,$(RAYLIB)/lib -lraylib

../build/cockpit_demo: cockpit_demo.c
	mkdir -p ../build
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# The demo loads assets/*.png, so it must run from the project root.
run: ../build/cockpit_demo
	cd .. && ./build/cockpit_demo

clean:
	rm -f ../build/cockpit_demo

.PHONY: run clean
