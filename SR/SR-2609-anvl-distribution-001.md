# SR-2609-anvl-distribution-001: publish `anvil-node` and `anvil-wasm` as externally installable/downloadable artifacts

**ID:** SR-2609-anvl-distribution-001
**Type:** Service Request (packaging / distribution)
**Owner:** anvil-node / anvil-wasm (one ask, identical underlying problem — see "Why one ask, not two" below)
**Filed:** 2026-09-20
**Requested by:** flywire (downstream consumer)
**Status:** open
**Tags:** distribution, packaging, npm, installability, anvil-node, anvil-wasm

---

## Summary

FlyWire depends on `anvil-node` and `anvil-wasm` today via local `file:` dependencies pointing at
sibling repo checkouts (`file:../anvil.node`, `file:../anvil.wasm` in `flywire-protocol`'s
`package.json`) — this only works because both repos happen to be checked out next to
`flywire-protocol` on whichever machine runs `npm install`. That's fine for FlyWire's own private
`origin` development, but is a hard, structural blocker for the FlyWire project's planned public
`dist` distribution: a `dist`-published FlyWire package can never be `npm install`able by anyone
outside this dev layout unless `anvil-node` and `anvil-wasm` themselves become reachable by
someone who has never seen this machine or this specific sibling-repo arrangement.

This SR asks that both become genuinely installable/downloadable independent of local
sibling-checkout wiring — the "where"/"how" is explicitly not decided here (see "Open question"
below); this SR exists to name the requirement and record what's actually blocking it on each
side.

## Why one ask, not two

Same underlying problem for both — a `file:` dependency that only resolves inside one specific dev
machine's directory layout — even though the concrete fix differs in difficulty between them (see
below). Filed together for the same reason `FR-2609-anvl-public-api-001` treated `anvil-node`/
`anvil-wasm` as "one ask, identical surface": tracking them separately would just duplicate the
same framing and the same open question twice.

## Two distinct technical problems, not one

- **`anvil-wasm` is the easier half.** Its build output (`anvil.js` + `anvil.wasm`, produced via
  Emscripten) is a portable, platform-independent artifact once built — nothing further needs to
  compile on an installer's machine, unlike a native addon. The real gap today is narrower than it
  looks: that output isn't published anywhere outside this dev checkout's gitignored `dist/` (see
  `flywire-protocol/scripts/sync-anvil-wasm.js` for how it currently only ever moves between two
  local sibling checkouts) — it just needs to actually ship somewhere an external consumer's
  install step can reach, with no further build-toolchain requirement on their end.

- **`anvil-node` is the hard half.** It's a native addon built via node-gyp, currently compiled
  from source at install/build time against a local build toolchain (gcc/g++/make/python3 — all
  present on the FlyWire dev machine and the Linode deploy box used for `demo.flywireprotocol.com`,
  not guaranteed to be present on an arbitrary external consumer's machine at all). Making this
  genuinely externally installable means either:
  1. shipping prebuilt binaries for the target platforms (the standard answer for a public native
     npm package — `prebuildify`/`node-pre-gyp` are the usual tools, downloading the right
     platform's prebuilt `.node` file at install time instead of compiling), or
  2. requiring the consumer to have a full C build toolchain themselves and compile at install
     time (works, but a much rougher install experience, and a real new dependency — a compiler —
     for anyone who just wants to `npm install` something).

  Option 1 is the real fix if broad installability is the goal; option 2 is what happens today by
  default (inside this dev layout) and isn't good enough for a public audience.

## Open question — where these actually get hosted, and how consumers reach them

Not decided here, deliberately — flagged directly by the requester as still under discussion, with
one concrete idea floated: a dedicated `ftp.anvldata.com` for direct-link, curl-able downloads of
both artifacts.

One real technical constraint worth weighing in that decision, checked directly against npm's own
dependency resolver (`npm-package-arg`, `lib/npa.js`), not assumed: **npm's dependency resolution
does not accept a plain `ftp://` URL as a valid dependency specifier.** Its protocol switch
recognizes `http:`/`https:` for a remote-tarball dependency, and separately recognizes
`git+ftp:`/`git+http:`/`git+https:`/`git+ssh:`/etc. for git-based dependencies — but a bare
`ftp:` URL (not `git+ftp:`) falls through to `unsupportedURLType` and throws `EUNSUPPORTEDPROTOCOL`.

Practical implication: an FTP host would work perfectly well for a human manually downloading and
installing either package (a browser, `curl ftp://...`, a custom install script that fetches and
unpacks by hand) — but it would **not** let a consumer's `package.json` reference either package as
a normal dependency and have plain `npm install` resolve it, since npm itself can't fetch an
`ftp://` dependency URL at all. That specific "a downstream project just runs `npm install` and it
works" experience needs either:
- publishing to the real npm registry (the standard answer — also gets versioning, integrity
  hashes, and `npm install anvil-wasm@x.y.z` for free, no custom install-script needed on the
  consumer's side), or
- the same artifact also mirrored over plain HTTPS, so `package.json` can point a dependency at an
  `https://.../anvil-wasm-x.y.z.tgz` remote-tarball URL directly (npm supports this natively — no
  registry publish needed, but also none of a registry's versioning/integrity conveniences).

Worth deciding in light of which install experience actually matters most for FlyWire's own
`dist`-published package: a plain `npm install flywire` that resolves everything transitively with
zero manual steps (needs the registry or an HTTPS tarball URL, not FTP), versus a documented
manual-download-and-place step being acceptable for now (FTP, or any static host, is fine for
that).

## Verification

Not applicable yet — this is an open request, not a resolution. No implementation work has started
on either side.
