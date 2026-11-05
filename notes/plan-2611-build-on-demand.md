# Plan 2611, lane: native builds on demand (for performance users)

Part of `notes/plan-2611-portable-ports.md`. Status: proposed (2026-11-04). The owner calls the build-on-demand
tooling "a work in progress".

## Owner direction

"I'd like to just use the Linode instance to build on demand ... someone provides their parameters, we build and
package." Also: "we should definitely work out a build on demand for performance users ... we can prob'ly still work
all that through the `anvldata` site."

## What blocks a native build on any platform but Linux x86-64 today

Found 2026-11-04.
1. **Allocator.** `Makefile` links the prebuilt `/usr/local/packages/sigma.system.alloc.o` (an x86-64 ELF object that
   exists only on this machine) and includes headers from `/usr/local/include`. Its source is `sigma.core`'s
   `src/system_alloc.c`.
2. **Compiler.** It's gcc with `-std=c2x`. clang works on macOS. On Windows, use clang or `zig cc`, not MSVC (weak C23).
3. **Exports.** `libanvil.exports` is a GNU-ld version script. macOS needs `-exported_symbols_list`; Windows needs a
   `.def` file or `__declspec(dllexport)`.
4. **POSIX.** `src/core/files.c` (`fstat` and the like) is the only POSIX dependency. It needs a small Windows shim.
5. **Packaging.** Every binding packs only `linux-x64`: NuGet `runtimes/linux-x64/native`, anvil.py the same layout,
   and anvil.java builds from source on the consumer's machine.

## Tasks

| Task | Work | Done when |
|---|---|---|
| B1 (FR-2611-anvl-build-001) | Build the allocator from source inside native's build, with no `/usr/local` dependency | a clean clone builds on a machine without `/usr/local/packages` |
| B2 (FR-2611-anvl-build-002) | Platform-neutral exports: generate the version script, the exported-symbols list or the `.def` file from one list | `check-exports` passes per platform |
| B3 (FR-2611-anvl-build-003) | Windows file shim for `files.c` | native suite passes on Windows |
| B4 (FR-2611-anvl-build-004) | Cross-compile matrix with `zig cc` from the Linode box: linux-x64, linux-arm64, osx-arm64, osx-x64, win-x64 | reproducible `libanvil` per RID, built twice and byte-identical |
| B5 (FR-2611-anvl-build-005) | Test where runnable: linux-x64 and arm64 on Linode, macOS and Windows on real machines or hosted runners. **An artifact never tested on its platform is labelled untested.** | native suite plus binding suites per RID |
| B6 (FR-2611-anvl-build-006) | Packaging per binding: NuGet `runtimes/<rid>/native/`, Python platform wheels, jar resources per `os-arch` extracted at load (FR-2611-anvl-java-001) | each binding's release checks per RID |
| B7 (FR-2611-anvl-build-007) | Linode build worker: a request with whitelisted parameters (version tag, binding, RID), a queue, an isolated build, SHA256SUMS, and an artifact store | builds a requested combination unattended; parameters are never free text |
| B8 (FR-2611-anvl-build-008) | anvldata.com request flow: a form for the parameters, the request status, and a download link with checksum, keeping the published-downloads rules | a user gets a checksummed build without engineer involvement |
| B9 (FR-2611-anvl-build-009) | Security review: whitelist-only parameters, no user-supplied code, rate limits, auth or not (owner decision), and artifact retention | review signed off |

## Decisions for the owner

- O1. The platform list, and which platforms count as tested versus build-only.
- O2. Whether build requests need an account or approval, and the retention of built artifacts.
- O3. Where macOS and Windows tests run: hosted runners, or machines the owner provides.

## Order

B1 to B3 are pure native work and help everyone, so they can start after v0.14.0-rc. B4 to B6 follow. B7 to B9 are
the service, and wait on the owner's decisions O2 and O3.
