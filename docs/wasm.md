# Anvil WebAssembly Build and Web-Client Integration

This document defines what the current wasm output is, what it is not, and how web-client ownership is split between the core repo and binding/app repos.

## Current Output

From this repository:

- `make -C lib wasm` produces `lib/wasm/libanvil.a`
- `make -C lib all` produces debug, release, wasm, and bindings artifacts

`lib/wasm/libanvil.a` is an Emscripten static archive containing wasm-target object code. It is a link input for downstream packaging.

## What `lib/wasm/libanvil.a` Is Not

It is not yet:

- A final browser-loadable `.wasm` module
- A JS/TS loader package
- A runtime API contract by itself

A final web build requires a downstream link/package step (typically in a binding/app repo) that exports symbols and emits loader artifacts.

## Ownership Model

Core repo (`anvil`):

- Owns the canonical C implementation and wasm-target static archive generation
- Owns stable C ABI headers and semantics

Binding/app repo (`anvil.js`, FlyWire client, Anvil.UX):

- Owns final web packaging (`.wasm`, JS glue, module format, bundler integration)
- Owns framework/runtime ergonomics (Node/browser wrappers, TS facade, distribution)

This split keeps one source of truth for parser semantics while allowing each consumer to ship environment-specific runtime packaging.

## Stable Web-Facing C ABI (Proposed)

For web-client AML parsing, a stable ABI should be a small C shim layer with plain exported C functions. This avoids exposing internal vtable layouts directly to JS/WASM callers.

Suggested minimal ABI surface:

- `anvl_web_ctx_create(const char *src, size_t len, int dialect)`
- `anvl_web_ctx_dispose(void *ctx)`
- `anvl_web_parse(void *ctx)`
- `anvl_web_error_code(void)`
- `anvl_web_error_line(void)`
- `anvl_web_error_col(void)`
- `anvl_web_statement_count(void *ctx)`
- `anvl_web_statement_type(void *ctx, size_t i)`
- `anvl_web_statement_ident_pos(void *ctx, size_t i)`
- `anvl_web_statement_ident_len(void *ctx, size_t i)`
- `anvl_web_statement_value_type(void *ctx, size_t i)`
- `anvl_web_statement_value_pos(void *ctx, size_t i)`
- `anvl_web_statement_value_len(void *ctx, size_t i)`

Optional traversal extension:

- statement field/element count and indexed accessors
- value-level field/element traversal for nested structures

ABI stability rules:

- `int32_t`/`uint32_t` and `uint64_t` in ABI signatures only (no `usize` in exported contract)
- append-only API growth
- versioned exported symbol set
- avoid exporting internal struct layouts

## Why This Works for FlyWire and Anvil.UX

- AML parsing remains native C logic, compiled once to wasm
- Web client can parse locally without server round-trips
- Zero-copy span semantics are preserved conceptually (offset/length access in source buffer)
- Downstream repos can build UX-specific APIs while preserving one canonical parser behavior

## Readiness Snapshot

Current state:

- wasm-target core archive generation: ready
- final browser runtime packaging: owned downstream
- stable web C shim ABI: recommended next implementation step
