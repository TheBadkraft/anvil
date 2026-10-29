# Release Engineer: v0.10.0-rc

The Release Engineer's standing assignment for `v0.10.0-rc`. Role and rules:
`.claude/agents/anvl-release-engineer.md`. This file was `agents/ANVL-RELEASE.agent.md` until the
move to sub-agents on 2026-10-29.

**Tickets** (Rivet):
- SR-2610-anvl-distribution-001;
- BR-2610-anvl-distribution-001;
- SR-2609-anvl-distribution-001, which closes with the owner's OK. Hosting is done, and the
  Linux-x64-only scope is a recorded, deliberate deferral.

## Owner decisions (2026-10-26)

| # | Decision |
|---|---|
| R1 | **The release is `v0.10.0-rc`.** Core and every binding ship at that version. The local `Anvil.Net` `0.10.0-dev.1` package was provisional and sorts below it. |
| R2 | **The website is the Technical Writer's.** The downloads move to `v0.10.0-rc`, and the `0.8.0-rc` files are removed, not archived. Git history keeps them. |

## Status (2026-10-29)

| Step | State |
|---|---|
| 1. Audit the published `0.8.0-rc` tarballs; record results in BR-2610-anvl-distribution-001 | done |
| 2. `scripts/release/` packaging and checks, RED first | done |
| 3. Cut core: `0.10.0-rc`, `[Unreleased]` → `[v0.10.0-rc]`, merge, tag on `main` | done. Tag `v0.10.0-rc` on `e512b92`, BUILD 284, **local only**: pushing it needs `AUTHORIZED: push tag v0.10.0-rc` |
| 4. Each binding on `release/v0.10.0-rc`: pin, version, clean build, package, every check | done (node, wasm, py, net; java is not distributed) |
| 5. Stage the artifacts, `SHA256SUMS`, release notes and handoff | done, in `~/release-staging/v0.10.0-rc/`, handoff at `handoff/HANDOFF-documentor.md` |
| 6. After the Technical Writer's deploy: the live check against anvldata.com, then resolve the three tickets and update the changelogs | **open**, waiting on the deploy |

Staged SHA256s (also in `SHA256SUMS`):

```
32e52f3979659ecc56f652e5cedc739856ea0a90a60bb66558c13380b4c45880  anvil-v0.10.0-rc-linux-x86_64.tar.gz
0f4990601ee4d92dc74284ab96c1fc802e213df336609b7c7bfebc0a8d716ccc  anvil-node-v0.10.0-rc-linux-x86_64.tar.gz
10eb6cc989d0a9615c951a941602d148a02caee76bb25c8168e3fe85021047d9  anvil-wasm-v0.10.0-rc.tar.gz
de6596406ad838a532bc65332b83d79efa3ff6d7145f0c951f5dd55b77dfab99  anvil-py-v0.10.0-rc-linux-x64.tar.gz
ecaffbd1fc88b13b5a51164d320d1cbc8d38c6981f0890ab3c685d054f91c470  anvil-net-v0.10.0-rc-linux-x64.tar.gz
```

## Open owner question

Should `BR-2610-anvl-bindings-004` ship in `v0.10.0-rc`? It makes `parse()` throw on a trailing
`5.`, in node `58f00c6` and wasm `f5726ce`. If yes, node and wasm are rebuilt and repackaged, the
checks rerun, and `SHA256SUMS` and the handoff are updated before the deploy. No published file
is overwritten, because nothing is published yet.
