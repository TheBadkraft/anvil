#!/usr/bin/env python3
"""PreToolUse(Bash) guard: heavy runtimes must run under a systemd memory cap.

Agents run inside VS Code's process group. Twice (a 28 GB fuzzer, a 26 GB node profile) one
uncapped command was OOM-killed and took VS Code and every agent with it. This hook refuses a
Bash command that starts node, python3, npm, npx, make or dotnet on a script or target unless
the command line carries `systemd-run ... -p MemoryMax=...` (agents/STANDARDS.md section 8).
Small inline checks (node -e/-p, python3 -c) and version queries stay allowed.
"""
import json, re, shlex, sys

HEAVY = {"node", "nodejs", "python", "python3", "npm", "npx", "make", "dotnet"}
WRAPPERS = {"taskset", "nice", "ionice", "timeout", "env", "exec", "time", "command", "stdbuf", "sudo"}
SAFE_FLAGS = {"-", "-e", "-p", "-c", "--eval", "--print", "-v", "-V", "--version", "-h", "--help", "-n", "-q"}


def first_words(command):
    # Split into simple commands on shell separators; heredoc bodies become their own lines
    # and only count if a line itself starts with a heavy runtime.
    for seg in re.split(r"\|\||&&|[;&|\n()]", command):
        seg = seg.strip()
        if not seg:
            continue
        try:
            words = shlex.split(seg, posix=True)
        except ValueError:
            words = seg.split()
        i = 0
        while i < len(words):
            w = words[i]
            if re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", w):      # VAR=value prefix
                i += 1; continue
            base = w.rsplit("/", 1)[-1]
            if base in WRAPPERS:
                i += 1
                # skip the wrapper's own options and numeric args (taskset -c 6,7 / timeout 60)
                while i < len(words) and (words[i].startswith("-") or re.match(r"^[0-9][0-9,.:smh-]*$", words[i])):
                    i += 1
                continue
            yield base, words[i + 1:]
            break


def heavy_use(command):
    for base, args in first_words(command):
        if base not in HEAVY:
            continue
        if not args or args[0] in SAFE_FLAGS:
            continue
        return base
    return None


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        return 0
    command = (data.get("tool_input") or {}).get("command") or ""
    if "systemd-run" in command and "MemoryMax" in command:
        return 0
    tool = heavy_use(command)
    if not tool:
        return 0
    reason = (
        f"Refused: `{tool}` runs without a memory cap. Agents share VS Code's process group, and an uncapped "
        "runaway kills VS Code and every agent (twice so far). Re-run it as: systemd-run --user --scope -q "
        "-p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=<s> -- <command>  (foreground; agents/STANDARDS.md "
        "section 8). Inline checks (node -e, python3 -c) and --version are allowed uncapped."
    )
    print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse",
                                             "permissionDecision": "deny",
                                             "permissionDecisionReason": reason}}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
