# core/Makefile — Phase 0: CLI only, no library yet
CC      = cc
CFLAGS  = -std=c2x -Wall -Wextra -Werror -O3 -march=native

all: build_cli

build_cli: cli/main.c
	$(CC) $(CFLAGS) $< -o anvil_cli

clean:
	rm -f anvil_cli

test:
	@echo "No tests yet — parser not born."

.PHONY: all clean test