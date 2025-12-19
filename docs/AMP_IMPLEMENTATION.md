================================================================================
BLOB SCALAR ENCODING IMPLEMENTATION - COMPLETE
================================================================================

Date: December 19, 2025
Branch: v.0.1.0-source-interrogators
Commits: 2
  1. enforce: no whitespace between blob tag and opening backtick
  2. feat: implement blob scalar encoding for AMP dialect

================================================================================
PHASE 1: ENFORCEMENT
================================================================================

Purpose: Ensure tag position is deterministic by rejecting whitespace between
tag and backtick. Required for efficient bit-packed encoding.

Changes (parser.c):
- Added explicit check in parse_blob() after tag parsing
- Rejects any whitespace between @tag and opening backtick
- Throws ANVL_ERR_PARSER_UNEXPECTED_TOKEN if rule violated

Testing:
- All 51 parser tests pass
- All 20 meta-buffer tests pass
- No existing tests depend on whitespace between tag and backtick

================================================================================
PHASE 2: BLOB SCALAR ENCODING
================================================================================

NEW DIALECT: ANVL_DIALECT_AMP (Anvil Messaging Protocol)

Constants Added (types.h & constants.h):
- ANVL_SHEBANG_AMP = "#!amp"
- ANVL_DIALECT_AMP enum value
- ANVL_DIALECT_AURORA enum value (for sample files)

Helper Functions (types.h):
These inline functions provide clean API for encoding/decoding blob metadata:

  uint8_t blob_tag_length(usize encoded_length)
    - Extracts tag length from upper 8 bits
    - Returns: 0-255

  usize blob_content_length(usize encoded_length)
    - Extracts content length from lower 56 bits
    - Returns: 0-72 PB (practically unlimited)

  usize blob_encode_length(uint8_t tag_length, usize content_length)
    - Encodes both values into single usize
    - Used during blob parsing

ENCODING SCHEME:
  blob_meta.pos = content position (unchanged)
  blob_meta.len = (tag_length << 56) | (content_length & 0x00FFFFFFFFFFFFFF)

  Byte layout (64-bit little-endian):
    Byte 7: tag_length (0-255)
    Bytes 0-6: content_length (0-72 PB)

Parser Changes (parser.c):

parse_blob() refactored with dialect-aware logic:
- Array-based (AML, ASL, AURORA):
  * Tag as element [0]: ANVL_VALUE_BLOB
  * Content as element [1]: ANVL_VALUE_BLOB
  * Collection value returned
  
- Scalar-based (AMP):
  * Single ANVL_VALUE_BLOB value
  * Tag length encoded in length field
  * Scalar value returned

Dialect Detection (context.c):

source_parse_dialect() updated:
- Recognizes "#!amp" shebang
- Returns ANVL_DIALECT_AMP when detected
- Maintains shebang precedence over file extension

================================================================================
DESIGN ADVANTAGES
================================================================================

1. Backward Compatibility:
   - AML/ASL/AURORA continue using 2-element array encoding
   - No changes to existing code paths
   - Parser automatically selects encoding based on dialect

2. Efficiency (AMP):
   - Single allocation per blob (vs. 2 for array)
   - No array overhead
   - 56-bit content length sufficient for all practical uses

3. Tag Position Deterministic:
   - No whitespace allowed between @tag and backtick
   - Tag position = content position - tag_length
   - Enables reliable post-parse resolution

4. Clean API:
   - Helper functions hide bit-packing complexity
   - Clear semantics: blob_tag_length(), blob_content_length()
   - One-line encoding: blob_encode_length()

================================================================================
TESTING RESULTS
================================================================================

Parser Tests: 51/51 PASS
Meta-Buffer Tests: 20/20 PASS

Specific tests verified:
- test_blob_tag_metadata() - Both tagged and bare blobs
- Whitespace enforcement between tag and backtick
- AML/ASL dialects maintain 2-element array structure
- No breaking changes to existing functionality

================================================================================
NEXT STEPS
================================================================================

1. Implement AMP-specific parser restrictions:
   - Reject objects, arrays, tuples (scalars + blobs only)
   - Reject inheritance (inherits: invalid)
   - Reject attributes (@[...] invalid)
   - Reject vars and interpolation

2. Create AMP test suite:
   - Valid AMP messages (scalars, blobs)
   - Invalid syntax rejection tests
   - Performance benchmarks vs. AML

3. Implement AnvilScript (ASL) dialect:
   - Most permissive dialect
   - Function definitions
   - Full feature set

4. Performance measurement:
   - Compare AMP scalar blobs vs. AML array blobs
   - Benchmark parsing, memory usage, throughput
   - Document performance characteristics

================================================================================
NOTES
================================================================================

Tag Length Limitation (255 bytes):
- Highly unrealistic to exceed in practice
- Reasonable tag names: @json, @markdown, @html, @base64
- Even complex tags like @custom-serialization-format < 50 bytes
- Mark Twain reference: "unlikely anyone's writing novels in tags"

Content Length Limitation (56-bit / 72 PB):
- Effectively unlimited for any realistic use case
- Larger than any single file system
- Not a practical constraint

Whitespace Rule Enforcement:
- Enforced in parser, not configurable
- Ensures deterministic tag position
- Required for efficient encoding
- Tests confirm no existing code violates this rule

================================================================================
