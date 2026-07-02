# Bindings Sign-Off Checklist (Pinned)

Use this checklist before shipping or updating official bindings.

## Contract Integrity

- [ ] Binding builds against current C headers in `include/`.
- [ ] `docs/bindings-api-reference.md` matches current exported C API.
- [ ] `docs/Anvil C Users Guide.md` and `docs/changelog.md` are updated for any binding-facing API changes.
- [ ] If version/build changed, both C Users Guide and changelog were updated in the same change set.

## Runtime Behavior

- [ ] Parse/load/error APIs match documented semantics.
- [ ] Value-type mapping parity is verified (scalar/object/array/tuple/blob/varref/interp).
- [ ] Resolver behavior parity is verified for inheritance merges.
- [ ] Schema behavior parity is verified (`resolve`, `get_type`, `validate`).

## Diagnostics and Safety

- [ ] Error fidelity validated: code/message/line/column/path preserved.
- [ ] Memory safety checks run for native layer (valgrind/asan as applicable).

## Conformance and Compatibility

- [ ] Shared fixture conformance tests pass between C and binding layers.
- [ ] Supported runtime versions (Node/Python/.NET) are explicitly documented.
- [ ] A handoff manifest is generated (`make -C bindings contract`) and attached to review/release notes.

## Team Sign-Off

- [ ] Core C maintainer approval.
- [ ] Binding owner approval (Node/Python/.NET as applicable).
- [ ] Documentation owner approval for any contract/documentation changes.
