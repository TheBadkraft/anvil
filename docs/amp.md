# AMP — Anvil Messaging Protocol
**A dialect of Anvil (ANVL) · Specification Reference**  
**Status: Production-Ready (C reference) · v0.4.3-equivalent (Anvil.Net)**

---

## Licensing

Anvil is proprietary software. Copyright © 2025 Quantum Override. All rights reserved.

| Component | Included | Tier |
|-----------|----------|------|
| `anvil.o` — core parser | Parse AML, AMP, ASL; all public API headers | **Tier 1 — Core** |
| `anvil.security.o` — security module *(⚠️ Planned)* | Cipher registry, AMP Secure Packet spec, envelope validation, `@amp.*` tag namespace | **Tier 2 — Security** *(add-on; requires Tier 1)* |
| This specification | Protocol design, AMP grammar, blob/tag conventions | Proprietary — all rights reserved |

> Contact: [TBD]  
> [TODO] Full commercial license terms to be published.

---

## The Pitch

Every other messaging protocol — MQTT, AMQP, protobuf, gRPC — has an opinion about the wire format and forces the parser to care about encoding. AMP does not.

**The parser does not know or care whether a message is plaintext or ciphered. The dialect does not change. The parser does not change.**

> One parser. Any encoding. Any transport. Any cipher scheme the consumer chooses.

That is the licensable claim. AMP is a protocol specification that sits on top of any transport layer and delegates all encoding decisions to the consumer. The wire can be open and human-readable, or fully encrypted — the same parser handles both because blobs are opaque by design.

**The parser also holds no data.** Its entire internal state is integer offsets — byte positions and lengths into the original source buffer you provided. No content is ever copied, buffered, or resident inside the parser itself. Blob payloads are skipped entirely: the parser records where they start and how long they are, then moves on without reading a single byte of content. Under adversarial conditions — heap dump, memory probe, fuzzer, side-channel — the parser cannot expose payload data because it holds none.

> This is the security cross-section claim: the parser is not a liability in the trust chain. It has no surface to attack.

---

## What AMP Is

| Property | Statement |
|----------|-----------|
| **Dialect of ANVL** | Activated by `#!amp` shebang; same parser core as AML and ASL |
| **Transport dialect** | Scalars and blobs only — designed for structured messaging, not data modeling |
| **Cipher-agnostic** | Blob payloads are opaque to the parser; encoding belongs to the consumer |
| **Zero-copy / zero-buffer** | All values are integer spans (offset + length) into the original source buffer; no content is ever copied into the parser; parser internal state contains no user data |
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

## UDP-Friendly Fragmentation

AMP messages may be split across UDP datagrams using a `parts` tuple — an application-layer fragmentation hint requiring no transport header changes.

```
#!amp
parts        := (1, 3)    // this is part 1 of 3
msg_id       := "a3f9"    // correlates fragments at reassembly
payload      := @aes256-gcm`<fragment-ciphertext>`
```

| Field | Type | Role |
|-------|------|------|
| `parts` | scalar tuple `(index, total)` | Fragment index (1-based) and total fragment count |
| `msg_id` | string | Message correlation ID — same value across all fragments |
| `payload` | blob | This fragment's content; reassembly is consumer responsibility |

The parser handles `parts` as an ordinary scalar tuple. Reassembly, ordering, and timeout logic belong to the consumer. AMP rides on any datagram or stream transport unchanged.

> **Implementation note**: `parts` uses scalar tuple syntax, which is ⚠️ pending in the Anvil C reference parser (Anvil.Net v0.4.3 delta). Until then, express as two scalars: `part_index := 1` / `part_total := 3`.

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

---

## Security Customization (C API)

> **Tier 1 (Core License):** blob span helpers in `types.h`  
> **Tier 2 (Security License, ⚠️ Planned):** `anvil.security.o` — cipher registry, Secure Packet validation

AMP's cipher-agnosticism is a structural property of the parser, not a runtime policy. The parser hands you integer spans; you decide what they mean. This section describes how to build a complete security stack on that foundation in C.

---

### Security Boundary

| AMP guarantees | The consumer owns |
|---------------|-------------------|
| Correct blob tag and content spans (zero-copy into original buffer) | Cipher selection and key management |
| Forbidden construct rejection at parse time | Decoding, decryption, and verification |
| Flat, ordered statement list with type tag per statement | Nonce/IV generation and storage |
| Single error slot with code, line, column | Trust establishment and key exchange |
| No parser-internal copy of any payload byte | Retry, timeout, and transport policy |

The parser holds no data — see [The Pitch](#the-pitch). There is no plaintext surface inside the parser to attack.

---

### Blob Spans — Core API (Tier 1)

After parsing, every blob statement exposes its tag and content length via the inline helpers in `types.h`. All spans are zero-copy: offsets into the original `src` buffer passed to `anvil_parse()`.

```c
#include <anvil/types.h>

/* stmt->value_meta->type == ANVL_VALUE_BLOB */
const struct anvl_value_meta *vm = stmt->value_meta;

uint8_t    tag_len     = blob_tag_length(vm->len);     /* upper 8 bits  */
usize      content_len = blob_content_length(vm->len); /* lower 56 bits */
const char *content    = src + vm->pos;                /* first byte of blob content */
/* tag text: (content - tag_len - 1) when tag_len > 0 */
```

Nothing is allocated. `src` is the original buffer you handed to the parser.

---

### Tag Namespace Conventions

**`@amp.*` — proposed reserved namespace (Tier 2, ⚠️ Proposed):**

| Tag | Purpose |
|-----|---------|
| `@amp.enc` | Encrypted payload (AMP Secure Packet) |
| `@amp.sig` | Asymmetric signature blob |
| `@amp.mac` | HMAC / message authentication code |
| `@amp.key` | Wrapped or public key material |

The `@amp.*` namespace is **proposed** — not yet enforced by the parser. Reservation is contractual under the Tier 2 license.

**Consumer-space convention (Tier 1, no restrictions):**

| Example tag | Meaning |
|-------------|---------|
| `@aes256-gcm` | AES-256-GCM encrypted payload |
| `@chacha20-poly1305` | ChaCha20-Poly1305 |
| `@hmac-sha256` | HMAC-SHA256 integrity |
| `@ed25519` | Ed25519 signature |
| `@jwt` | JSON Web Token |
| `@x509` | DER-encoded certificate |

Convention: `{algorithm}` or `{algorithm}-{mode}`. No tag is forbidden by the parser.

---

### Cipher Registry Pattern *(Tier 2 — `anvil.security.o`, ⚠️ Planned)*

`anvil.security.o` provides a dispatch table mapping blob tag strings to consumer-supplied handler functions. All spans remain zero-copy.

```c
#include <anvil/security.h>

/* Resolved blob — zero-copy spans into original source buffer */
typedef struct {
    const char *tag;         /* ptr into src; NULL if untagged */
    usize       tag_len;
    const char *content;     /* ptr into src                   */
    usize       content_len;
} amp_blob_span_t;

/* Consumer-supplied decode function. Returns 0 on success. Caller owns *out_buf. */
typedef int (*amp_cipher_fn)(
    const amp_blob_span_t *blob,
    const void            *key_ctx,   /* consumer-managed key context */
    void                 **out_buf,
    usize                 *out_len);

typedef struct { const char *tag; amp_cipher_fn fn; } amp_cipher_entry_t;
typedef struct { amp_cipher_entry_t *entries; usize count; } amp_cipher_registry_t;

/* Resolve a parsed blob statement into an amp_blob_span_t */
amp_blob_span_t amp_resolve_blob(const char *src, const anvl_statement *stmt);

/* Walk registry, match tag, invoke handler */
int amp_dispatch_blob(
    const amp_cipher_registry_t *registry,
    const amp_blob_span_t       *blob,
    const void                  *key_ctx,
    void                       **out_buf,
    usize                       *out_len);
```

Usage:

```c
static int my_aes_handler(const amp_blob_span_t *b, const void *kctx,
                           void **out, usize *out_len) { /* AES-256-GCM decrypt */ }

amp_cipher_entry_t    entries[] = { { "aes256-gcm", my_aes_handler } };
amp_cipher_registry_t reg       = { entries, 1 };

for (usize i = 0; i < result->statement_count; i++) {
    anvl_statement *stmt = &result->statements[i];
    if (stmt->value_meta->type == ANVL_VALUE_BLOB) {
        amp_blob_span_t blob = amp_resolve_blob(src, stmt);
        void *plaintext; usize plain_len;
        amp_dispatch_blob(&reg, &blob, key_ctx, &plaintext, &plain_len);
        /* plaintext is decrypted content; caller frees */
    }
}
```

---

### Key/Session Metadata Pattern

Carry cipher context as scalar fields; keep the encrypted content in a single blob. The parser treats all statement types identically — no fields are privileged.

```
#!amp
amp_version  := 1
key_id       := "k-20260313-prod-01"
nonce        := 0xA1B2C3D4E5F60001
timestamp    := 1741824000
payload      := @aes256-gcm`<ciphertext>`
```

| Field | Type | Role |
|-------|------|------|
| `amp_version` | integer | Protocol version negotiation |
| `key_id` | string | Key lookup — indexes into consumer key store |
| `nonce` | hex | Per-message nonce / IV |
| `timestamp` | integer | Replay protection window |
| `payload` | blob `@aes256-gcm` | Encrypted content — parser-opaque |

All scalar fields are available immediately after parsing, before any decryption. The consumer loads the key by `key_id`, uses `nonce` as IV, and hands `content` to the cipher.

---

### Layered Security Pattern

Multiple blobs in a single message create independent security layers. Each blob is a separate, independently-parsable zero-copy span.

```
#!amp
amp_version  := 1
key_id       := "k-20260313-prod-01"
nonce        := 0xA1B2C3D4E5F60001
timestamp    := 1741824000
payload      := @aes256-gcm`<ciphertext>`
integrity    := @hmac-sha256`<mac-over-all-fields>`
attestation  := @ed25519`<signature-over-header-scalars>`
```

Recommended processing order:

1. Verify `@ed25519` attestation over the scalar header fields
2. Verify `@hmac-sha256` over the full raw message bytes
3. Decrypt `@aes256-gcm` payload using `key_id` and `nonce`

Each step uses its own entry in the cipher registry. The parser delivers all three blobs in a single parse pass.

---

### Full-Envelope Encryption

When field names themselves must be concealed, use a single blob whose content is an entire encrypted inner AMP (or AML) message. The outer message exposes only the minimum metadata for key resolution.

```
#!amp
amp_version  := 1
key_id       := "k-20260313-prod-01"
envelope     := @aes256-gcm`<encrypted-inner-amp-message>`
```

After decryption, re-parse the plaintext content in a second parse pass:

```c
anvl_result outer;
anvil_parse(src, src_len, &outer, ANVL_DIALECT_AMP);

amp_blob_span_t env = amp_resolve_blob(src, &outer.statements[2]);
void *inner_src; usize inner_len;
amp_dispatch_blob(&reg, &env, key_ctx, &inner_src, &inner_len);

/* same parser, second pass */
anvl_result inner;
anvil_parse(inner_src, inner_len, &inner, ANVL_DIALECT_AMP);
```

Trade-off: `key_id` and `amp_version` remain visible in the outer message. If those must also be concealed, a static pre-shared key resolves the envelope with no outer metadata at all.

---

### AMP → AML Handoff

AMP carries the transport envelope; AML models structured payload content. After decryption, no second network hop is needed — re-feed the decrypted bytes to the same parser in AML mode.

```
Outer AMP parse (shebang dispatch → AMP mode)
    └─ extract payload blob → zero-copy content span
           └─ consumer decrypt → plaintext bytes
                  └─ anvil_parse(plaintext, len, &result, ANVL_DIALECT_AML)
                         └─ full AML object tree available
```

Useful when the transport must be AMP (flat, fast, cipher-agnostic) but the payload is a rich AML configuration or data document requiring object/inheritance resolution.

---

### AMP Secure Packet *(Tier 2 — `anvil.security.o`, ⚠️ Planned)*

An **AMP Secure Packet** is a formally-defined AMP message conforming to the Secure Packet specification included with the Tier 2 license. A conformant packet:

- Uses only `@amp.*` tags for security-layer blobs *(proposed namespace)*
- Requires `amp_version`, `key_id`, and `timestamp` scalar fields
- Includes at minimum an `@amp.enc` payload blob
- Optionally includes `@amp.sig` and/or `@amp.mac` blobs

`anvil.security.o` provides a single structural validation entry point:

```c
typedef struct {
    usize           version;
    const char     *key_id;    usize key_id_len;
    usize           timestamp;
    amp_blob_span_t payload;   /* @amp.enc — required */
    amp_blob_span_t sig;       /* @amp.sig — optional */
    amp_blob_span_t mac;       /* @amp.mac — optional */
} amp_secure_envelope_t;

/* Returns 0 if parsed result is a structurally valid AMP Secure Packet.
 * Checks field presence, tag namespace, and version range only.
 * Cryptographic verification is the consumer's responsibility. */
int amp_verify_envelope(
    const char            *src,
    const anvl_result     *result,
    amp_secure_envelope_t *out_env);
```

`amp_verify_envelope` validates structure only — field presence, `@amp.*` tag compliance, version range. All cryptographic operations (decrypt, verify MAC, verify signature) are always the consumer's responsibility.
