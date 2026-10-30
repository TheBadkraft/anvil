# The ANVL team

As of 2026-10-29 the ANVL team is **one Claude Code session with sub-agents**. The session is the
**ANVL Principal Engineer**, the project lead. It starts a sub-agent for each task, in the role
that owns the work. Sub-agents last for one task and report only to the Principal Engineer.

- **Role definitions:** `.claude/agents/anvl-*.md` (model, tools, scope). The Principal Engineer
  starts each one with its `subagent_type` set to the role's file name.
- **Shared rules:** `agents/STANDARDS.md`. Every role reads it first.
- **Standing assignments** that outlive one task: `agents/assignments/`.

## Roles

| Role | Sub-agent | Rivet handle | Model | Replaces |
|---|---|---|---|---|
| **Principal Engineer** | (the session) | `anvl-engineer.badkraft` | Opus | ANVL Engineer |
| **Core Engineer** | `anvl-core-engineer` | `anvl-coder.badkraft` | Sonnet | ANVL Coder |
| **Bindings Engineer** | `anvl-bindings-engineer` | `anvl-bindings.badkraft` | Sonnet | ANVL Bindings |
| **AnvilScript Language Engineer** | `anvl-asl-language-engineer` | `anvl-scribe.badkraft` | Sonnet | Scribe |
| **AnvilScript Runtime Engineer** | `anvl-asl-runtime-engineer` | `anvl-runner.badkraft` | Sonnet | Runner |
| **Verification Engineer** | `anvl-verification-engineer` | `anvl-verification.badkraft` | Sonnet | *new* |
| **Release Engineer** | `anvl-release-engineer` | `anvl-release.badkraft` | Sonnet | ANVL Release |
| **Technical Writer** | `anvl-technical-writer` | `anvl-documentor.badkraft` | Sonnet | ANVL Documentor |
| **JS Engineer** | `anvl-js-engineer` | `anvl-js.badkraft` | Sonnet | *new*: anvil.js, shipped as `anvil-node` |
| **Triage Analyst** | `anvl-triage-analyst` | `anvl-triage.badkraft` | Haiku | ANVL Triage |

The Rivet handles are unchanged, so ticket history carries over.

## The Principal Engineer

The project lead, and the only role the owner talks to. It:
- owns planning, architecture and the roadmap;
- decides on every ANVL ticket, core and bindings (owner decision, 2026-10-29). It is principal
  engineer in Rivet for every ANVL project;
- sets priorities and the backlog;
- starts sub-agents, merges every branch after verification, and pushes on the owner's go;
- keeps the downstream teams (FlyWire, Rivet) informed and answers their questions.

## How a task runs

1. The Principal Engineer picks a ticket and starts the owning role. The task prompt names the
   ticket, the branch, the worktree, and any `AUTHORIZED:` lines.
2. The role works in its own worktree, to `agents/STANDARDS.md`, and reports back.
3. The **Verification Engineer** checks the branch: the diff against the ticket, the full suites,
   the changelog. It returns a pass or a list of failures.
4. On a pass, the Principal Engineer merges, pushes when authorized, and notifies downstream:
   - **FlyWire** gets a notice on every round where anvil.node/anvil.wasm are pushed and the Linode
     box is rebuilt.
   - **Rivet** gets the notices it was promised.

Work for another role goes through Rivet and the Principal Engineer, never directly from one role
to another. Independent tasks (a core fix and a binding fix, say) can run in parallel in separate
worktrees.

## Approvals

The owner gives approvals to the Principal Engineer, which passes them on as an
`AUTHORIZED: <action>` line in the task prompt. That covers tag pushes, deploys, repo pushes and
Linode box updates. A sub-agent never takes one of those actions without the line (owner decision,
2026-10-29).
