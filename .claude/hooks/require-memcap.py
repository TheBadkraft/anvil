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


SEPARATORS = r"\|\||&&|[;&|\n()]"


def split_segments(command):
    """Split into simple commands on shell separators that are outside quotes: a `|` in `grep -E "a|node x"` is a
    pattern, not a pipe. If the quotes do not balance (an apostrophe in a heredoc body, say), fall back to splitting
    everywhere, which can only make the hook stricter."""
    segs, cur, quote, i = [], [], None, 0
    while i < len(command):
        c = command[i]
        if quote:
            cur.append(c)
            if c == "\\" and quote == '"' and i + 1 < len(command):
                i += 1
                cur.append(command[i])
            elif c == quote:
                quote = None
        elif c in "'\"":
            quote = c
            cur.append(c)
        elif c == "\\" and i + 1 < len(command):
            cur.append(c)
            i += 1
            cur.append(command[i])
        else:
            m = re.match(SEPARATORS, command[i:])
            if m:
                segs.append("".join(cur))
                cur = []
                i += len(m.group(0))
                continue
            cur.append(c)
        i += 1
    if quote:
        return re.split(SEPARATORS, command)
    segs.append("".join(cur))
    return segs


def first_words(command):
    # Heredoc bodies become their own lines and only count if a line itself starts with a heavy runtime.
    for seg in split_segments(command):
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
