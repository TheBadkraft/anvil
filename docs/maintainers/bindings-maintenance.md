# Bindings Maintenance Policy

This document defines how official bindings are maintained in this repository.

## Official Scope

Any binding under `bindings/*/` is official.

Official binding workspaces:

- `bindings/node/`
- `bindings/python/`
- `bindings/dotnet/`

## Build Integration

- Core libraries are built via `lib/Makefile`.
- Bindings generation and handoff are orchestrated via `bindings/Makefile`.
- A core build (`make -C lib`) also triggers `make -C bindings generate`.

This keeps binding artifacts and contracts fresh without turning one Makefile into
an oversized monolith.

## Required Documentation Sync

When binding-facing APIs change, update all three:

1. `docs/bindings-api-reference.md`
2. `docs/Anvil C Users Guide.md`
3. `docs/changelog.md`

If build/version is updated, `docs/Anvil C Users Guide.md` and
`docs/changelog.md` must be updated in the same change set.

## Handoff Between Teams/Agents

Use:

```sh
make -C bindings contract
```

This writes:

- `bindings/.handoff/binding-handoff.json`

The manifest captures the current git revision, version macros, and checksums of
binding-critical headers/docs so another team (e.g. anvil.js) can verify they are
working from the same contract snapshot.

## Sign-Off Gate

Before release of any official binding, run through:

- `docs/maintainers/bindings-signoff-checklist.md`
