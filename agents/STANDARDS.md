# ANVL team standards

Every ANVL sub-agent reads this file at the start of every task. It holds the rules that used to
live in session memory. A sub-agent starts cold, so anything not written here or in its role file
does not exist for it.

The team is one session: the **ANVL Principal Engineer** (planning, architecture, merges) and the
sub-agents it starts for single tasks. See `agents/README.md` for the roles.

---

## 1. Chain of command

- **You report only to the Principal Engineer**, in your final message. You don't message other
  sessions, other roles or downstream teams (FlyWire, Rivet). The Principal Engineer relays.
- **Your task prompt is your assignment.** It names the ticket, the branch and the worktree. Do
  that task and nothing else. When you find more work, file it (§4) and list it in your report.
- **Owner approvals arrive as `AUTHORIZED: <action>` lines** in your task prompt. Tag pushes, site
  deploys, pushes of any repo, and updates to the Linode box need one. Without that exact line,
  stop before the action and report that it is ready. An `AUTHORIZED` line covers only the action
  it names, for this task only.
- **Stop and report** instead of guessing when:
  - the ticket and the code disagree;
  - a design question isn't settled in the ticket or the notes;
  - a gate fails for a reason outside your write scope;
  - finishing would need a change outside your write scope.

## 2. Git

- **Never switch branches in a shared checkout** (`~/Projects/repos/anvil` or any binding's main
  checkout). Work in the worktree your task names. If it names none, create one:
  `git worktree add -b <branch> ../<repo>.worktrees/<name> main`.
- Run `git branch --show-current` and `git status` before every commit. Never commit files you
  didn't write.
- Branch names: `fix/<BR-ID>`, `feat/<FR-ID>`, `chore/<SR-ID>`, `docs/<topic>`, `release/vX.Y.Z[-tag]`.
- **You don't merge to `main`.** The Principal Engineer merges after the Verification Engineer
  passes the branch.
- Leave underscore-prefixed legacy files (`src/core/_*.c`) alone. A missing one is usually the
  owner's deliberate deletion; ask before restoring it.

## 3. Development (strict TDD)

1. **Write the full test set for the requirement first.**
2. **Reach a genuine RED.** The test binary builds and runs, and every new case fails on an
   assertion. A case that fails to compile, link or run (a segfault, say) is not RED. Stub the
   functions under test so they compile and return a default.
3. **Implement test by test, RED to GREEN.** Don't write the whole implementation first; the
   ordering keeps scope from creeping.
4. **Never change a test's input to dodge a failure.** When a test and the code disagree, decide on
   the merits which one is wrong, and fix that one.
5. **Run the full suite after your last commit, not just the new tests.** Report the counts.
   - **Native:** every suite in `test/unit/` (`make test_<suite>`) and each under valgrind
     (`make test_<suite> val`). Then, from the repo root: `make check-exports check-tsan
     check-version check-reader-only check-test-release-fresh`.
   - **Bindings:** that binding's full test command.
6. In `test/unit/`, a suite is the target and the action is a second goal: `make test_header`
   (build and run), `make test_header build`, `make test_header val`. A bare `make test` does
   nothing.
7. VM and bytecode work uses larger, functional test cases. The RED-first ordering still applies.

## 4. Tickets (Rivet only)

- All tickets live in Rivet. `BR/`, `FR/` and `SR/` (and `anvil.java/FR/`) are frozen history:
  read them, never edit them.
- Use **only your own role's handle**. Never read another role's credentials file.
- **Put the credentials inline on every Rivet command, reads and writes alike.** Never use a separate
  `export`. Each Bash call is a fresh shell, and the permission check looks at one command at a time. So a write
  whose own command line doesn't name the credentials is denied ("Identity Not Given"), even after an earlier
  export. This is the Rivet Project Engineer's guidance, 2026-10-30. Run `whoami` first:
  ```sh
  RIVET_CREDENTIALS_FILE=~/.config/rivet/agents/anvl-<role>.anvl rivet whoami   # e.g. anvl-coder.anvl; handle anvl-<role>.badkraft
  RIVET_CREDENTIALS_FILE=~/.config/rivet/agents/anvl-<role>.anvl rivet resolve <ID> --resolution - <<'EOF'
  ...
  EOF
  ```
  `rivet` is on PATH. If it isn't, use `node ~/Projects/repos/rivet-live/src/cli/main.js` in the same command;
  don't rely on an alias. If a Rivet write is denied, fix the command and retry it yourself. Never ask another
  agent to do it for you.
- The guide is `~/Projects/repos/rivet-live/docs/agent-guide.md`. Read §3, "The one ANVL gotcha":
  a backtick blob ends at the first backtick inside it, so add sections containing backticks with
  `rivet section set <ID> <key> --from f.md`.
- Work on your ticket: `rivet transition <ID> in-progress` at the start; comments with progress;
  `rivet resolve <ID> --resolution f.md` and a comment
  `branch …, head …, tests N/N` at the end.
- File new work for another role in Rivet: project `anvil` for native work; `anvil.node`,
  `anvil.py`, `anvil.net` or `anvil.java` for binding work; `anvil.js` for the pure-JS implementation. (`anvil.node` and `anvil.wasm` are frozen: file nothing new there.) List it in your report.
  Don't assign it.
- Questions belong on the ticket (`rivet block`, or a comment with an @mention), not in your
  report alone.

## 5. Finish rules (every task)

- **Changelog.** Every finished task ends with a changelog entry: in anvil, `docs/changelog.md`
  under `## [Unreleased]` (Keep a Changelog headings: `### Added`, `### Changed`, `### Fixed`, …);
  in a binding, its own changelog. Name the ticket ID and the user-visible effect. Planning and
  documentation work count.
- **Notes.** Check `notes/deferred-work.md` at the start of a task. Treat every note as a claim to
  verify against the code, not as ground truth. Mark a superseded note in place.
- **Report** in your final message:
  - what changed;
  - the branch and head commit;
  - the test counts;
  - any tickets you filed;
  - anything that needs the owner or the Principal Engineer.

## 6. Design principles (owner rulings)

- **Primitives, not policy.** Anvil Native provides general tools; consumers (FlyWire, Rivet, …)
  build their own policy on top. A consumer's code is evidence that a need is real, not the spec.
- **A binding adapts to native**, using its own language's idioms. Change native only for a real
  defect that doesn't depend on one binding. Examples: an ABI gap, a wrong result, a
  memory-safety issue, or the same friction in two unrelated paradigms.
- **The official bindings stay consistent.** Same names, same shapes, same semantics. Bindings
  differ only where their runtime forces it (wasm's async `ready()`).
- **Internal code is always the optimal choice; the public API trusts the caller** with general
  tools.
- **Enumeration uses Sigma's HPS iterator** (`include/sigma/query.h`; an opaque
  `…_iterator_next`/`…_iterator_dispose` cursor), not count + index accessor pairs. Read
  `query.h` and `query.c` directly before deciding.
- **Reuse `include/constants.h` tokens** directly. Never add an alias constant.
- **A public module gets its own specific error codes**, one per reason, numbered independently.
- **"No demonstrated demand" is not an argument against a feature.** Argue from cost, API surface,
  reversibility or consistency.
- The vendored `src/sigma/` subset is ours to research with. Draft Sigma requests in
  `../sigma.anvil/FR|BR/`, never in the upstream Sigma repos.

## 7. Public text

Nothing published (the `site/` directory, release notes, public changelog) names ticket IDs,
`notes/` paths, agents or roles, private ops (hosts, internal paths, Linode details), or the
internal projects Rivet, Sigma, CabNet and q-or. **FlyWire may be named.** The owner treats it as
deliberate marketing. `docs/` and `notes/` are internal and may name anything.

## 8. Infrastructure

- **Linode box** (`ssh linode`, root): `emcc` lives there, so anvil.wasm builds there
  (`source ~/emsdk/emsdk_env.sh && ./build.sh`). FlyWire's server follows the box's checkouts
  `/root/projects/anvil.node` and `/root/projects/anvil.wasm`. To update one:
  `git pull --ff-only && git submodule update --init`, then build and test. That needs an
  `AUTHORIZED` line. `vendor/anvil` comes from GitHub, so the pinned core commit must be pushed
  first.
- **CI**, when it comes, is built on the Linode box, not a hosted service.
- **Browsers: only through anvil.wasm's contained harness**, `test-browser/run-browser.sh <chromium|firefox> <glue|bundle>`
  (install once with `test-browser/install-browsers.sh`; prove with `test-browser/prove-harness.sh`). It runs each
  browser in a systemd user scope (1.5 GB memory, 256 processes, 180 s), one run at a time, with all state in
  `/tmp/anvl-browser-test/`. **Never launch a browser directly, never use snap Firefox, never wrap one in `timeout`.**
  On 2026-10-30, leaked headless browsers filled the owner's home directory and froze the workstation.
- **Heavy or long-running commands run under a hard memory cap.** That covers fuzzing, benchmarks, corpus
  generation, big builds, and anything that loops over thousands of cases or runs more than about a minute.
  Run them as
  `systemd-run --user --scope -p MemoryMax=4G -p MemorySwapMax=0 -p RuntimeMaxSec=<s> -- <command>`
  (lower the cap if you can). **Agents run inside the owner's VS Code process group.** On 2026-10-30 a fuzz
  run grew to 28 GB, and the out-of-memory killer took VS Code and every agent down with it, twice. Never
  start background jobs (`&`, `nohup`, `setsid`) that outlive your command; run in the foreground under the
  cap, in bounded chunks. Generators must cap the size of each case, and drivers must stream results.
- **Every speed figure names both sides and the direction.** Write ratios as "<subject> vs <baseline>
  (<version or commit>)", in time taken, and say which way is better: "anvil.js vs anvil-node v0.10.0-rc:
  0.26x time (lower is faster)", or "native acb4602 vs native v0.11.0-rc: 1.007x". Never give a bare "1.02x" or
  "0.5x". Put the label on every table header, report, changelog entry and ticket. Native-vs-previous-native
  (the 1.02x gate), anvil.js-vs-anvil-node (the ≤1.00x gate) and anvil.js-vs-`JSON.parse` (a target, not a
  gate) are different comparisons and must never share a table without labels.
- **The cap is enforced, not just asked for (2026-10-31).** A project hook, `.claude/hooks/require-memcap.py`, refuses
  any Bash command that starts `node`, `python3`, `npm`, `npx`, `make` or `dotnet` on a script or target without
  `systemd-run … -p MemoryMax=…` on the same command line. `node -e`, `python3 -c`, `python3 -` heredocs and `--version`
  are allowed. VS Code's scope also runs with `OOMPolicy=continue`, so a runaway kills only itself. Don't work around
  either guard. On 2026-10-30 an uncapped `node` profiling script grew to 26 GB and took VS Code and every agent down.
- **The owner's home directory is not scratch space.** Put temporary state in your session scratchpad or under
  `/tmp`, clean it up on exit, and leave no processes running. Before reporting, `ls -A ~` must be unchanged and
  `pgrep -fa 'firefox|chrom|playwright'` must show nothing of yours.
- **anvldata.com** deploys with `cd site && npx wrangler deploy`. The command publishes everything
  on disk under `site/` except what `.assetsignore` lists, so run `ls -la` first. Only the Technical
  Writer deploys.
