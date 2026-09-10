# ANVL — Front Page Draft

***Attributed** · **Node** · **Variadic** · **Language***

---

## Hero

# One language, three problems resolved.
**Most stacks solve with three separate tools.**

Config, structured messaging, and (soon) embedded scripting — one grammar, one parser, no exceptions. Anvil (Native), the reference implementation, is a single C parser with no runtime dependencies; bindings - currently Node & WASM - wrap it directly, so a document parses identically no matter which one reads it.

[Download →](/download) [Read the docs](/docs) [Sandbox](/sandbox)

---

## Why we are

JSON exists so that ANVL could live.

JSON was never designed — it was lifted out of a subset of JavaScript's own object literals and standardized after the fact, because it had already won on convenience. Nobody ever got to ask "is this good enough?" The answer was assumed. That's the whole problem: good enough to ship is exactly the kind of decision that costs you later, quietly, after everyone's already built on top of it.

Anvil started from the opposite direction — out of frustration with Makefile's DSL, a look at CMake that made the climb steeper instead of easier, and a JSON experiment that went nowhere. What began as "can this be a config language" turned out to be an object modeling language. Strip a few features back out, and it's also a stable, deterministic messaging protocol. Anvil's dialects aren't three separate designs bolted together — they're the same grammar with different amounts of it turned on.

There's a lot more to it than a couple of lines can give credit for. [Read the full story →](/story)

---

## Why you need it

**A bare `no` in your config silently becomes the boolean `false`.** YAML's Norway problem doesn't announce itself — it just quietly means the wrong thing until something breaks in production, maybe months later. ANVL's grammar never guesses: a literal is only `true`, `false`, or `null` if it's exactly that word. A mistake fails to parse instead of silently meaning something else.

**A message schema starts as five scalar fields and quietly grows a nested object.** Nobody reviews that kind of drift until a downstream consumer breaks on structure it was never built to handle. AMP, Anvil's restricted dialect, forbids objects, attributes, inheritance, and imports outright — rejected at parse time, not by convention or code review. The payload can't grow arbitrary structure even by accident.

**A fuzzer or a heap dump can only expose what a parser actually held.** Anvil Native's internal state is integers — byte offsets and lengths into the buffer you hand it. No string is copied during parsing, and blob payloads are skipped entirely. Under adversarial conditions, the parser can't leak payload data, because it never held any.

---

## A look at AML

```
#!aml
server @[env = production] := {
    host := localhost;
    port := 8080;
    tags := [edge, cache];
};

// variables can be defined and referenced
var_ref    := 42;
derived_var := $var_ref;
```

---

## Why not just JSON? *(reference table — moved below the fold)*

| | JSON | ANVL |
|---|---|---|
| Comments | Not allowed | `//` and `/* */` |
| Reference a value you already defined | Copy-paste, or a non-standard `$ref` | Native `$identifier`, resolved once at parse time |
| Extend or override a template | Not supported | Single inheritance — `Child : Base := {...}` |
| Restricted subset for untrusted-ish payloads | None — any valid JSON is valid JSON | AMP: scalars and flat arrays/tuples only, rejected otherwise at parse time |
| Metadata attached to a field | Invent your own convention | Native `@[key=value]` attributes |
| Bare identifiers as values | Always quoted strings | Unquoted wherever unambiguous |

---

## Try it now

[Download](/download) the native library or the WebAssembly bundle and parse real source in minutes — see the [Bindings guide](/docs#Bindings-Guide.md) for a runnable example in each. A live in-browser [sandbox](/sandbox) is in progress.

Anvil Native — the reference C implementation of AML/AMP, with Node.js and WebAssembly bindings. v0.8.0-rc.

---

### Notes / open questions for review
- Confirm placement of the status/honesty table (README's "this README used to claim more than the codebase did") — could be a strong trust-building callout on `/docs` or even teased here.
- "ASL — design stage" should probably get one honest line on the front page rather than silence, so nobody assumes scripting exists today.
- Need final call on whether the comparison table stays this far down or gets its own anchor link from the hero.
