# Plan 2611, lane: Java (`anvil.java`, pure Java)

Part of `notes/plan-2611-portable-ports.md`. Status: proposed (2026-11-04).

## Today

- `~/Projects/repos/anvil.java` (`linode:repos/anvil.java.git`): FFM (`java.lang.foreign`) over `libanvil.so`, JDK 25.
- Since 2026-11-03 (BR-2611-anvl-java-001) it builds native from its own `vendor/anvil` at v0.13.1-rc and checks the
  version at load. The suite is 135/135.
- Coordinates are `dev.badkraft.anvil:anvil.java`. It's never published.
- **Known consumer:** `~/Projects/badkraft.mcdeuce`, a Minecraft mod framework, through
  `includeBuild("../repos/anvil.java")`. Minecraft players are mostly on Windows and macOS, so this is where a
  portable Java implementation matters most.
- **Limitation:** a fat jar records an absolute path to the native library (FR-2611-anvl-java-001).

## Harness phase (proven against the current FFM binding)

| Task | Work | Done when |
|---|---|---|
| JAVA-H0 (FR-2611-anvl-java-002) | Bring the binding to its v0.14 mirror (drift test), so the reference passes against the native it pins | suite green |
| JAVA-H1a-d (FR-2611-anvl-java-003) | A conformance CLI (Gradle `application`, or a test-scoped main) with reader, writer, builder and schema modes, printing the four dump formats through the public API | reader 1902, writer, builder (C-API-only lists exact) and schema 561, byte-exact |
| JAVA-H2 (FR-2611-anvl-java-004) | A fuzz driver against `anvil-dump` | recorded seeds, 0 differences |
| JAVA-H3 (FR-2611-anvl-java-005) | Drift test against `conformance/drift-tables.txt` (the existing `VtableDriftTests` extended to codes, names and messages) | green |
| JAVA-H4 (FR-2611-anvl-java-006) | check-tdd **gradle adapter**: `./gradlew --no-daemon test` JUnit XML; `AssertionFailedError` vs other throwables; product `src/main`, tests `src/test` | the checker's fixtures prove it; one real range checked (for example, BR-2611-anvl-java-001) |
| JAVA-H5 (FR-2611-anvl-java-007) | Bench harness (JMH, or a plain harness matching the shared workloads), cold and warm, labelled | baseline numbers for the FFM binding |
| JAVA-H6 (FR-2611-anvl-java-008) | check-examples Java support (compile and run a sample against the jar) | Java samples in the docs run |

**Harness gate:** JAVA-H1-H5 green against the FFM binding, independently verified.

## Port phase (after owner approval)

| Task | Scope |
|---|---|
| JAVA-R0 | Rename the binding: repo `anvl-java-binding`, coordinates `dev.badkraft.anvil:anvl-java-binding` (proposed). **mcdeuce moves its `includeBuild` path first.** New empty `anvil.java` repo. |
| JAVA-P1 | Reader and read API (pure Java over `byte[]` or `ByteBuffer`) |
| JAVA-P2 | Writer |
| JAVA-P3 | Builder |
| JAVA-P4 | Types and schema |
| JAVA-P5 | A pure jar with no native or `native-pin.properties`, plus the release checks. mcdeuce then moves to the port. |

**Java-specific notes**
- The port needs no native toolchain and no `--enable-native-access`, so it fixes the fat-jar limitation by
  construction.
- `long` covers int64; use `BigInteger` where native allows larger values.
- **Speed:** FFM calls are cheap, so the pure port may be slower than the binding. H5 measures it first (D3).
- **Java version:** decide the port's minimum. JDK 25 is what the binding needs for FFM; a pure port could target an
  older LTS that Minecraft's runtime ships.
