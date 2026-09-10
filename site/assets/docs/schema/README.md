# AnvilSchema

Status: **implemented** — `@[schema]` loading, required-field presence, type-kind validation,
and full constraint checking (`size`/`min`/`max`/`values`, including inheriting a constraint
from a resolved `types.X` custom type when the field doesn't declare its own), collecting every
violation in one pass. See the [Language Reference](../language-reference.md#10-schema) and
[Getting Started](../getting-started.md) guide for the hands-on side; this page is the
introduction — the questions worth answering before anyone gets far enough into the technical
detail to think to ask them.

## The question nobody asks about their schema tool, until it's too late

Every format that's succeeded long enough eventually needs a way to say "this document must
look like *this*." XML got there. JSON got there. Every one of them solved it the same way:
by inventing something new.

XML got XSD — its own W3C specification, its own grammar, its own processor, entirely separate
from the XML parser that reads the document XSD is describing. Want to transform that document
instead of just validating it? That's XSLT — a *third* specification, its own declarative
language, its own processor again. Three technologies, three grammars, three things to learn,
none of which the original XML parser has ever heard of.

JSON's story is the same shape, just later. JSON shipped with no validation concept at all —
`{"age": 42}` is exactly as valid as `{"age": "not a number"}` as far as any `JSON.parse()` in
the world is concerned. JSON Schema arrived years afterward, an external specification — written
*as* JSON, which is almost a joke — that no JSON parser has ever been taught to read. Validating
against it means reaching for a second library entirely, maintained by a different project, on a
different release schedule, with no structural guarantee it even stays in step with whatever
`JSON.parse()` itself considers valid JSON.

Nobody asks "wait, why does validating my data require an entirely separate piece of software
from the thing that reads my data?" They just accept it, because every format they've ever used
works this way. It's not a law of nature. It's just what happens when schema gets bolted on
*after* the format already shipped without one.

## What's actually different here

A `.schema.anvl` file is parsed by the exact same `anvil_load()` call, running the exact same
grammar, as any other `.anvl` file on earth. There is no second parser. There is no second
specification. We checked, directly, against the real parser, before writing a single line of
schema-specific code: every piece of grammar a schema file needs — attributes on any statement at
any nesting depth, ordinary objects, imports — was already there, fully implemented and tested,
before this design thread even started. Zero new grammar was required. Not "we kept it minimal."
Zero.

That's not a coincidence and it's not a clever trick. It's what falls out of building one
language, one grammar, one parser in the first place, instead of a format plus a growing pile of
things bolted onto it later. A schema document doesn't need special syntax because it was never
going to need a special *anything* — it's just data, describing other data, the same way every
other ANVL document describes whatever it describes.

## AnvilSchema is a consumer, not a feature

Here's the part worth sitting with: Anvil Native doesn't have schema support built in. It never
will, exactly. What it has is a public API — read a document's statements, read their values,
read their attributes, walk what they import — available in full to *any* consumer, for *any*
purpose. AnvilSchema is what you get when that generic API gets pointed at one specific job:
reading a document that says `@[schema]` and checking another document's shape against it.

Nothing about that job is privileged. Nothing about it required Anvil Native to change its own
rules, add a special code path, or make an exception for schema's sake. AnvilSchema is a
demonstration of what the primitives already support, not a capability someone had to go add.
That's the whole point of building primitives instead of policy: the library's job stops at
"parse correctly and expose what's there, generically, to anyone" — what any particular consumer
*does* with that is entirely up to them, using tools no more special than the ones AnvilSchema
itself uses.

## This is one demonstration, not the whole story

AnvilSchema is the first real answer to "what can you build on top of ANVL that ANVL itself
never had to know about." It won't be the last, and it isn't the deepest example available
either — a scripting language (AnvilScript) is next on the list, and it's built on the exact same
premise: nothing special, just the primitives, aimed at a different job. We haven't come close to
scratching the surface of what "one parser, generic primitives, extend however you need" actually
makes possible. AnvilSchema is just the first proof.
