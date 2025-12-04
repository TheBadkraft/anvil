#!/usr/bin/env bash
# setup-anvil.sh — Minimal project structure setup for Anvil
# Run from the anvil/ root directory: bash setup-anvil.sh

set -euo pipefail

echo "Aligning the forge..."

# Core C directories (matches README.md)
mkdir -p core/{include,src,tests,samples,cli}

# Touch empty placeholders
touch core/include/anvil.h
touch core/src/{anvil,parser,types,resolver,writer}.c
touch core/tests/main.c
touch core/cli/main.c
touch core/Makefile

# VS Code bare-bones
mkdir -p .vscode
cat > .vscode/tasks.json <<'EOF'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build libanvil.a (static)",
            "type": "shell",
            "command": "make -C core libanvil.a",
            "group": "build"
        },
        {
            "label": "Build libanvil.so (shared)",
            "type": "shell",
            "command": "make -C core libanvil.so",
            "group": "build"
        },
        {
            "label": "Build CLI",
            "type": "shell",
            "command": "make -C core cli",
            "group": "build"
        },
        {
            "label": "Run Tests",
            "type": "shell",
            "command": "make -C core test",
            "group": "test"
        },
        {
            "label": "Clean",
            "type": "shell",
            "command": "make -C core clean"
        }
    ]
}
EOF

cat > .vscode/launch.json <<'EOF'
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug CLI",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/core/anvil_cli",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "MIMode": "lldb",
            "preLaunchTask": "Build CLI"
        },
        {
            "name": "Run Tests",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/core/anvil_test",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "MIMode": "lldb",
            "preLaunchTask": "Run Tests"
        }
    ]
}
EOF

# Minimal Makefile for core/
cat > core/Makefile <<'EOF'
CC      = cc
CFLAGS  = -std=c23 -Wall -Wextra -Werror -O3 -march=native -flto -fPIC

SRC     = src/anvil.c src/parser.c src/types.c src/resolver.c src/writer.c
OBJ     = $(SRC:.c=.o)

all: libanvil.a libanvil.so cli

libanvil.a: $(OBJ)
	ar rcs $@ $^

libanvil.so: $(OBJ)
	$(CC) -shared $(CFLAGS) $^ -o $@

cli: libanvil.a cli/main.c
	$(CC) $(CFLAGS) cli/main.c libanvil.a -o anvil_cli

test: libanvil.a tests/main.c
	$(CC) $(CFLAGS) tests/main.c libanvil.a -o anvil_test
	./anvil_test

clean:
	rm -f $(OBJ) libanvil.a libanvil.so anvil_cli anvil_test

.PHONY: all cli test clean
EOF

echo "Forge aligned to canon."
echo "Drop .anvl files into core/samples/ and run make -C core test"
