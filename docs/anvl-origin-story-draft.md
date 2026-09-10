# The Origin Story — Draft

*Placeholder title — "How ANVL Happened" / "Why ANVL Exists" / "The Long Version"*

---

Anvil didn't start as an attempt to build a language. It started as an attempt to learn C — and getting blindsided by Makefile on the way in.

Makefile's syntax isn't really trying to be a language for humans. It's a build-dependency DSL that happened to calcify into universal use, and learning it felt like a detour before the detour: first learn C, then learn an entire second, unrelated language just to compile it. The frustration wasn't "this is hard" — it was "why does compiling C require fluency in something this disconnected from the problem."

CMake looked like the fix. It wasn't. It replaced one file with several, and traded one DSL for the job of understanding the difference between a `CMakeLists.txt` and everything else CMake expects you to track alongside it. Fewer headaches were not part of the deal — just different, multiplied ones.

So: what about a build configuration in JSON? That lasted about as long as it took to type the first pair of quotes. JSON has no comments, no way to define a value once and reference it elsewhere, nothing resembling the variable substitution Makefile at least got right. Modeling a real build — with anything like a reusable, inheritable shape — was a non-starter.

That's the moment the actual question changed. Not "what existing format can I bend into shape" but "what would I build if I designed this properly, for how I actually think about structure." One requirement was non-negotiable from the start: variable replacement. A build configuration without it is just a list of things you'll be retyping forever.

From there it kept unfolding. If a top-level entry could hold something like a set of paths, and another document could import it and inherit that entry — reuse it wholesale, override only what's different — then nothing ever needed to be rewritten or re-looked-up again. That was the moment it clicked: this wasn't a config format anymore. This was an object modeling language.

A quick look around confirmed it — object modeling languages are a real, established category, and UML (Universal Modeling Language) is the well-known one in that space. Anvil's design didn't set out to compete with UML — it arrived at object modeling by way of a build-config problem — but it belongs in the same conversation: a language for modeling objects and their relationships, arrived at from a completely different direction.

Once the object modeling side was solid, the rest moved fast. Strip a handful of features back out — no inheritance, no imports, no attributes, no nesting — and what's left is a stable, deterministic messaging protocol. Not a second language — the same grammar with fewer permissions turned on. That became AMP.

And once there were two dialects sharing one grammar, a third possibility opened up: instead of one giant, fixed scripting language, an extensible DSL model — dialects declared explicitly, enforced at the parser level, each one only as permissive as the problem actually requires. That's ASL, still in the design stage.

None of this means a build tool can be skipped entirely, either. Refuse to use one, and the alternative is writing raw GCC command lines by hand — which turns into a script that stitches those command lines together — which turns into wishing there were a config file so the same lines didn't have to be retyped for every new project. That's the whole loop, right back to square one. Makefile made that climb steeper than it needed to be. CMake didn't fix it — it just moved the headache into more files. And JSON, once it was actually tried, offered nothing to make the climb worth it — no comments, no reuse, no way to model what a real build config needs to model. Anvil doesn't erase the learning curve. It reshapes it into one that's actually productive to climb.

---

### Notes / open questions for review
- Naming Makefile/CMake/JSON/UML by name is confirmed intentional — pain points get specific reasons, not vague swipes; no direct-comparison harm claims are made.
- UML claim softened to "same conversation" / comparison, not displacement — confirmed per review, since UML's actual human-readability as a language (vs. a rendering standard for tooling) hasn't been researched enough to claim more.
- Placement: standalone `/story` page linked from front page tease, or folded into `/docs` intro? Current draft assumes standalone page.
