# Official Bindings Workspace

This directory hosts official Anvil bindings and their shared maintenance flow.

## Layout

- `bindings/node/` — Node/N-API (`anvil.js`) binding workspace
- `bindings/python/` — Python binding workspace
- `bindings/dotnet/` — .NET binding workspace
- `bindings/scripts/` — shared maintenance and handoff scripts

## Build and Generation

Run from repository root:

```sh
make -C bindings generate
```

This executes per-binding generation hooks (when present) and writes a handoff
manifest at `bindings/.handoff/binding-handoff.json`.

## Pull Node Binding Into anvil.js

If `anvil.js` lives as a sibling repository (`../anvil.js`), copy the built addon with:

```sh
make -C bindings pull-node
```

Default copy:

- source: `bindings/node/build/Release/anvil_binding.node`
- destination: `../anvil.js/build/Release/anvil.node`

## Contract and Sign-Off

- API reference: `docs/bindings-api-reference.md`
- Maintainer policy: `docs/maintainers/bindings-maintenance.md`
- Team sign-off checklist: `docs/maintainers/bindings-signoff-checklist.md`

Bindings in this directory are considered official and are expected to track the
current C API exported by headers in `include/`.
