# anvil — AI Coding Guidelines

## Agent Boundaries

- **Source edits**: Only modify files within this project's directory tree. Do NOT edit source files in other projects.
- **Reading**: May read files in sibling projects for context (headers, interfaces, documentation).
- **Tickets**: Every ticket (`TICKET-*`) must include a `## Test Cases` section with concrete inputs and expected outputs or assertions.

## Task Lifecycle

When implementing Feature Requests (FRs) or Bug Reports (BRs), follow the **mandatory 8-step lifecycle** documented in:

**[q-or/DOC_STANDARDS.md — Agent Task Lifecycle](../../q-or/DOC_STANDARDS.md#agent-task-lifecycle)**

**Before declaring any task complete, verify ALL 8 steps are done:**

1. ✅ UPDATE WIP (start) — Edit `q-or/wip/anvil.anvl`, mark in-progress, run `wip.py sync`
2. ✅ BRANCH — Create `feature/FR-###` or `bugfix/BR-###` branch
3. ✅ IMPLEMENT — TDD cycle (RED → GREEN → REFACTOR)
4. ✅ TEST — Full suite GREEN, valgrind clean, no regressions
5. ✅ DOCUMENT — CHANGELOG, README, inline docs, FR/BR resolution notes
6. ✅ COMMIT — Conventional commits, reference FR/BR ID, push branch
7. ✅ UPDATE FR/BR — Change status to "resolved", update `q-or/registry.anvl`
8. ✅ UPDATE WIP (complete) — Mark steps done, update next_action, run `wip.py sync`

**FR/BR Placement:** FRs and BRs targeting this project MUST live in `anvil/feature-reqs/` or `anvil/bug-reports/`, not in the reporter's project.

## Session Continuity

At session start, read `q-or/wip.anvl` (merged view). If anvil has active work, announce feature, completed steps, and `next_action`. To update: edit `q-or/wip/anvil.anvl`, then run `python3 tools/wip.py sync`.
