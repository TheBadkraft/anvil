# AMP — Anvil Messaging Protocol
**A dialect of Anvil (ANVL) · Specification Reference**  
**Status: Production-Ready (C reference) · v0.4.3-equivalent (Anvil.Net)**

---

## The Pitch

Every other messaging protocol — MQTT, AMQP, protobuf, gRPC — has an opinion about the wire format and forces the parser to care about encoding. AMP does not.

**The parser does not know or care whether a message is plaintext or ciphered. The dialect does not change. The parser does not change.**

> One parser. Any encoding. Any transport. Any cipher scheme the consumer chooses.

That is the licensable claim. AMP is a protocol specification that sits on top of any transport layer and delegates all encoding decisions to the consumer. The wire can be open and human-readable, or fully encrypted — the same parser handles both because blobs are opaque by design.

---

## What AMP Is

| Property | Statement |
|----------|-----------|
| **Dialect of ANVL** | Activated by `#!amp` shebang; same parser core as AML and ASL |
| **Transport dialect** | Scalars and blobs only — designed for structured messaging, not data modeling |
| **Cipher-agnostic** | Blob payloads are opaque to the parser; encoding belongs to the consumer |
| **Zero-copy** | All values are spans into the original source buffer; zero allocations on the hot path |
| **Parser-enforced restrictions** | Forbidden constructs are rejected at parse time — no runtime guard needed |
| **Licensable** | AMP is a specification, independently licensable from the larger Anvil toolchain |

---

## What AMP Is Not

| Statement | Why |
|-----------|-----|
| Not an encryption library | Cipher selection and key management are consumer concerns |
| Not a transport | AMP structures the message; delivery is the consumer's problem |
| Not a schema language | AML handles data modeling; AMP handles message transport |
| Not general-purpose ANVL | Objects, inheritance, and attributes are explicitly forbidden |

---

## The Three-Dialect Picture

AMP is one of three Anvil dialects. Together they cover every layer of a system:

| Dialect | Shebang | Domain | Purpose |
|---------|---------|--------|---------|
| **AML** | `#!aml` | Data & configuration modeling | Replaces JSON, YAML, TOML |
| **AMP** | `#!amp` | Communications modeling | Replaces MQTT, AMQP, protobuf wire formats |
| **ASL** | `#!asl` | Behavior modeling | Embedded scripting — TBD v0.2.0+ |

One parser. One language. Three complete layers of a distributed system.

---

## What AMP Allows

Every AMP message is a flat list of named scalar or blob statements.

### Allowed

| Construct | Example | Notes |
|-----------|---------|-------|
| String | `msg := hello world` | UTF-8 |
| Integer | `msg := 42` | |
| Float | `msg := 3.14` | |
| Boolean | `msg := true` | |
| Null | `msg := null` | |
| Hex literal | `msg := 0xFF` | |
| Bare literal | `msg := some_literal` | |
| Untagged blob | `` msg := `content` `` | Opaque payload |
| Tagged blob | `` msg := @json`content` `` | Tag is consumer metadata |

### Scalar arrays and scalar tuples *(Anvil.Net v0.4.3+)*

| Construct | Example | Notes |
|-----------|---------|-------|
| Scalar array | `msg := [1, 2, 3]` | Elements must be scalar — no nesting |
| Scalar tuple | `msg := (true, "ok", 0)` | Elements must be scalar — no nesting |

> **Implementation gap**: The Anvil C reference parser currently rejects all arrays and tuples. This brings it to parity with pre-v0.4.3 Anvil.Net behavior. Scalar array/tuple support is a known pending parser update.

### Forever Forbidden

| Construct | Error |
|-----------|-------|
| Objects `{}` | `AmpObjectNotAllowed` |
| Attribute syntax `@[…]` | `AmpAttributeNotAllowed` |
| Inheritance `:` | `AmpInheritanceNotAllowed` |
| `using` / `import` | `AmpImportNotAllowed` |
| Nested arrays/tuples | `AmpArrayElementNotScalar = 4401` |

These restrictions are permanent. They are what make AMP fast, predictable, and transport-appropriate.

---

## Blob Encoding (The Cipher Hook)

Blobs are the mechanism by which AMP achieves cipher-agnosticism.

```
@tag`content`
   ↑           ↑
   tag         payload (opaque to parser)
```

The parser encodes blob metadata as a single `usize`:

```
blob_meta.len = (tag_length << 56) | (content_length & 0x00FFFFFFFFFFFFFF)
```

- **Bytes 0–6**: content length (up to 72 PB — not a practical constraint)
- **Byte 7**: tag length (0–255 bytes)

**What this means for cipher schemes**: The tag identifies the encoding/cipher scheme. The content is the payload. The parser stores both as spans into the original buffer and hands them to the consumer. The consumer owns all decoding. The parser is structurally incapable of caring about what is inside a blob.

---

## Dialect Detection

Dialect is determined by shebang on the first line:

| Shebang | Dialect |
|---------|---------|
| `#!amp` | `ANVL_DIALECT_AMP` |
| `#!aml` | `ANVL_DIALECT_AML` (default) |
| `#!asl` | `ANVL_DIALECT_ASL` |

No shebang defaults to AML. Shebang takes precedence over file extension.

---

## Sample Message

```
#!amp
user_id     := 12345
username    := "alice"
is_active   := true
timestamp   := 1703071200
session     := @jwt`eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...`
payload     := @aes256`3q2+7w==`
```

The `@jwt` and `@aes256` tags are consumer-defined. The parser reads them, stores them, and asks no questions.

---

## Implementation Status

### Anvil C (`anvil/`) — v0.1.0-alpha

| Feature | Status |
|---------|--------|
| Dialect detection (`#!amp`) | ✅ Complete |
| Scalar parsing (all types) | ✅ Complete |
| Blob parsing (tagged + untagged) | ✅ Complete |
| Blob scalar encoding (upper-8 / lower-56) | ✅ Complete |
| Forbidden construct rejection | ✅ Complete |
| Scalar arrays and tuples | ⚠️ Pending (Anvil.Net v0.4.3 delta) |
| 17/17 AMP tests passing | ✅ |
| Zero memory leaks | ✅ |

### Anvil.Net (`anvil.net/`) — Frozen Reference

Anvil.Net is the authoritative specification reference. Its backend is frozen. When a feature exists in Anvil.Net core, it must be implemented in Anvil C.

| Feature | Anvil.Net Version | C Status |
|---------|-------------------|----------|
| AMP dialect core | v0.1.0 | ✅ Complete |
| Blob scalar encoding | v0.1.0 | ✅ Complete |
| Scalar arrays in AMP | v0.4.3 | ⚠️ Pending |
| Scalar tuples in AMP | v0.4.3 | ⚠️ Pending |

---

## Integration Contracts

### What AMP exposes to consumers

| Contract | Description |
|----------|-------------|
| Statement list | Flat, ordered list of `key := value` pairs |
| Value type | Scalar type tag per statement (string, int, float, bool, null, hex, blob) |
| Blob tag span | Consumer-readable tag identifying encoding/cipher scheme |
| Blob content span | Opaque content — consumer owns all decoding |
| Error slot | Single parse error with code, line, column |

### What AMP does NOT expose

| Item | Reason |
|------|--------|
| Decoded blob content | Parser is opaque by design |
| Cipher negotiation | Consumer contract, not parser contract |
| Transport framing | AMP structures messages; delivery is out of scope |
| Schema validation | AML concern; AMP is typeless by design |

---

## Relationship to the Rest of the Toolchain

```
AMP message arrives (any transport, any encoding)
    ↓
Anvil parser (C) — shebang dispatch → AMP mode
    ↓
Flat statement list — spans into original buffer (zero-copy)
    ↓
Consumer resolves blobs (decrypts, decodes, deserializes)
    ↓
Consumer may deserialize blob content into AML for further parsing
```

AMP and AML are complementary: AMP carries the envelope, AML models the payload. A ciphered AMP message can carry an AML document as its blob payload — parse the outer envelope with AMP, hand the payload blob to AML after the consumer decrypts it.
