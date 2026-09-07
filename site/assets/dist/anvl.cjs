var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// src/index.js
var index_exports = {};
__export(index_exports, {
  AnvilError: () => AnvilError,
  AnvilNode: () => AnvilNode,
  ErrorCode: () => ErrorCode,
  lastError: () => lastError,
  load: () => load,
  parse: () => parse,
  parseRawValue: () => parseRawValue,
  write: () => write
});
module.exports = __toCommonJS(index_exports);

// src/errors.js
var ErrorCode = Object.freeze({
  NONE: 0,
  // Parser errors (4001–4099)
  EXPECTED_IDENTIFIER: 4001,
  EXPECTED_ASSIGN: 4002,
  EXPECTED_VALUE: 4003,
  UNEXPECTED_TOKEN: 4004,
  EXPECTED_SCALAR: 4005,
  UNTERMINATED_STRING: 4006,
  UNTERMINATED_COMMENT: 4007,
  BARE_HASH_NOT_HEX: 4008,
  MALFORMED_VARREF: 4009,
  MISSING_STATEMENT_TERMINATOR: 4010,
  // Collection errors
  EMPTY_OBJECT_NOT_ALLOWED: 4011,
  EMPTY_ARRAY_NOT_ALLOWED: 4012,
  EMPTY_TUPLE_NOT_ALLOWED: 4013,
  TUPLE_TOO_FEW_ELEMENTS: 4014,
  INHERITANCE_REQUIRES_OBJECT: 4015,
  MALFORMED_EXPONENT: 4016,
  RESERVED_IDENTIFIER: 4017,
  // AMP dialect errors (4401)
  AMP_ARRAY_ELEMENT_NOT_SCALAR: 4401,
  AMP_VARREF_FORBIDDEN: 4402,
  AMP_OBJECT_FORBIDDEN: 4403,
  AMP_ATTRIBUTE_FORBIDDEN: 4404,
  AMP_INHERITANCE_FORBIDDEN: 4405,
  // I/O errors (100x)
  FILE_NOT_FOUND: 1002,
  INVALID_PATH: 1003,
  // General
  NOT_IMPLEMENTED: 9999
});
var ErrorNames = Object.freeze(
  Object.fromEntries(Object.entries(ErrorCode).map(([k, v]) => [v, k]))
);
var AnvilError = class extends Error {
  constructor(code, message, line = 0, column = 0, file = null) {
    super(message);
    this.name = "AnvilError";
    this.code = code;
    this.line = line;
    this.column = column;
    this.file = file;
  }
};

// src/source.js
var NEWLINE = "\n";
var CARRIAGE_RETURN = "\r";
var TAB = "	";
var SPACE = " ";
var Source = class {
  constructor(text, file = null) {
    this.text = text;
    this.file = file;
    this.pos = 0;
    this.line = 1;
    this.column = 1;
    this.length = text.length;
    this.lastError = null;
  }
  peek(offset = 0) {
    const idx = this.pos + offset;
    if (idx >= this.length) return "\0";
    return this.text[idx];
  }
  isEof() {
    return this.pos >= this.length;
  }
  isEofOffset(offset) {
    return this.pos + offset >= this.length;
  }
  advance() {
    if (this.isEof()) return "\0";
    const ch = this.text[this.pos];
    this.pos += 1;
    if (ch === NEWLINE) {
      this.line += 1;
      this.column = 1;
    } else {
      this.column += 1;
    }
    return ch;
  }
  consume(n = 1) {
    for (let i = 0; i < n; i++) {
      this.advance();
    }
  }
  match(str) {
    if (this.text.startsWith(str, this.pos)) {
      return str.length;
    }
    return 0;
  }
  mark() {
    return { pos: this.pos, line: this.line, column: this.column };
  }
  restore(mark) {
    this.pos = mark.pos;
    this.line = mark.line;
    this.column = mark.column;
  }
  slice(pos, len) {
    return {
      source: this,
      pos,
      len,
      toString() {
        return this.source.text.slice(this.pos, this.pos + this.len);
      }
    };
  }
  sliceFrom(mark) {
    const pos = mark.pos;
    const len = this.pos - mark.pos;
    return this.slice(pos, len);
  }
  isWhitespace(ch) {
    return ch === SPACE || ch === TAB || ch === NEWLINE || ch === CARRIAGE_RETURN;
  }
  isIdentifierStart(ch) {
    return ch >= "a" && ch <= "z" || ch >= "A" && ch <= "Z" || ch === "_";
  }
  isIdentifierPart(ch) {
    return this.isIdentifierStart(ch) || ch >= "0" && ch <= "9" || ch === "-";
  }
  isDigit(ch) {
    return ch >= "0" && ch <= "9";
  }
  isHexDigit(ch) {
    return this.isDigit(ch) || ch >= "a" && ch <= "f" || ch >= "A" && ch <= "F";
  }
  skipWhitespace() {
    while (!this.isEof() && this.isWhitespace(this.peek())) {
      this.advance();
    }
  }
  skipComment() {
    if (this.peek() === "/" && this.peek(1) === "/") {
      while (!this.isEof() && this.peek() !== NEWLINE) {
        this.advance();
      }
      return true;
    }
    if (this.peek() === "/" && this.peek(1) === "*") {
      const start = this.mark();
      this.advance();
      this.advance();
      while (!this.isEof() && !(this.peek() === "*" && this.peek(1) === "/")) {
        this.advance();
      }
      if (this.isEof()) {
        this.setError(ErrorCode.UNTERMINATED_COMMENT, "Unterminated block comment", start);
        return false;
      }
      this.advance();
      this.advance();
      return true;
    }
    return false;
  }
  skipWhitespaceAndComments() {
    while (!this.isEof()) {
      this.skipWhitespace();
      const hadComment = this.skipComment();
      if (this.lastError) return false;
      if (!hadComment) break;
    }
    return true;
  }
  setError(code, message, location = null) {
    if (this.lastError) return this.lastError;
    const loc = location || this.mark();
    const err = new AnvilError(code, message, loc.line, loc.column, this.file);
    this.lastError = err;
    return err;
  }
};

// src/parser.js
var DIALECT_AML = "aml";
var DIALECT_AMP = "amp";
var DIALECT_ASL = "asl";
var SHEBANG_AML = "#!aml";
var SHEBANG_AMP = "#!amp";
var SHEBANG_ASL = "#!asl";
function skip(source) {
  return source.skipWhitespaceAndComments();
}
function insertionPoint(source) {
  const mark = source.mark();
  return { line: mark.line, column: mark.column + 1 };
}
var RESERVED_IDENTIFIERS = /* @__PURE__ */ new Set(["vars", "import", "using", "true", "false", "null"]);
var RESERVED_AS_VALUE = /* @__PURE__ */ new Set(["vars", "import", "using"]);
function scanIdentifier(source) {
  if (!source.isIdentifierStart(source.peek())) return null;
  const mark = source.mark();
  while (!source.isEof() && source.isIdentifierPart(source.peek())) source.advance();
  const name = source.sliceFrom(mark).toString();
  if (RESERVED_IDENTIFIERS.has(name)) {
    source.setError(ErrorCode.RESERVED_IDENTIFIER, `'${name}' is reserved and cannot be used as an identifier`, mark);
    return null;
  }
  return name;
}
function isBareChar(ch) {
  return ch >= "a" && ch <= "z" || ch >= "A" && ch <= "Z" || ch >= "0" && ch <= "9" || ch === "_" || ch === "-" || ch === ".";
}
function isHexRun(source, n) {
  for (let i = 0; i < n; i++) {
    if (!source.isHexDigit(source.peek(i))) return false;
  }
  return !source.isIdentifierPart(source.peek(n));
}
function parseHexOrError(source) {
  const mark = source.mark();
  source.advance();
  if (isHexRun(source, 6)) {
    const digitsMark = source.mark();
    source.consume(6);
    const text = source.sliceFrom(digitsMark).toString().toUpperCase();
    return { type: "scalar", kind: "hex", value: parseInt(text, 16), hexString: text };
  }
  if (isHexRun(source, 3)) {
    const digitsMark = source.mark();
    source.consume(3);
    const text = source.sliceFrom(digitsMark).toString().toUpperCase();
    const expanded = text.split("").map((c) => c + c).join("");
    return { type: "scalar", kind: "hex", value: parseInt(expanded, 16), hexString: expanded };
  }
  source.setError(ErrorCode.BARE_HASH_NOT_HEX, "Bare # is not a valid hex color", mark);
  return null;
}
function parseQuotedString(source) {
  const startMark = source.mark();
  source.advance();
  let value = "";
  const escapeMap = { '"': '"', "\\": "\\", n: "\n", t: "	", r: "\r" };
  while (true) {
    if (source.isEof()) {
      source.setError(ErrorCode.UNTERMINATED_STRING, "Unterminated string", startMark);
      return null;
    }
    const ch = source.peek();
    if (ch === '"') {
      source.advance();
      break;
    }
    if (ch === "\\") {
      source.advance();
      if (source.isEof()) {
        source.setError(ErrorCode.UNTERMINATED_STRING, "Unterminated string", startMark);
        return null;
      }
      const esc = source.peek();
      value += escapeMap[esc] !== void 0 ? escapeMap[esc] : esc;
      source.advance();
      continue;
    }
    value += ch;
    source.advance();
  }
  return { type: "scalar", kind: "string", value };
}
var VALID_EXPONENT = /^-?\d+(\.\d+)?[eE][+-]\d+$/;
var EXPONENT_SHAPED = /^-?\d+(\.\d+)?[eE]/;
function parseBareToken(source) {
  const mark = source.mark();
  if (source.peek() === "-") source.advance();
  if (!isBareChar(source.peek())) {
    source.setError(ErrorCode.EXPECTED_SCALAR, "Expected a scalar value", mark);
    return null;
  }
  while (!source.isEof() && isBareChar(source.peek())) source.advance();
  const scannedSoFar = source.sliceFrom(mark).toString();
  if (/[eE]$/.test(scannedSoFar) && (source.peek() === "+" || source.peek() === "-")) {
    source.advance();
    while (!source.isEof() && source.isDigit(source.peek())) source.advance();
  }
  const raw = source.sliceFrom(mark).toString();
  if (raw === "true") return { type: "scalar", kind: "bool", value: true };
  if (raw === "false") return { type: "scalar", kind: "bool", value: false };
  if (raw === "null") return { type: "scalar", kind: "null", value: null };
  if (RESERVED_AS_VALUE.has(raw)) {
    source.setError(ErrorCode.RESERVED_IDENTIFIER, `'${raw}' is reserved and cannot be used as a value`, mark);
    return null;
  }
  if (/^-?\d+$/.test(raw)) return { type: "scalar", kind: "int", value: parseInt(raw, 10), raw };
  if (/^-?\d+\.\d+$/.test(raw)) return { type: "scalar", kind: "float", value: parseFloat(raw), raw };
  if (VALID_EXPONENT.test(raw)) return { type: "scalar", kind: "float", value: parseFloat(raw), raw };
  if (EXPONENT_SHAPED.test(raw)) {
    source.setError(
      ErrorCode.MALFORMED_EXPONENT,
      "Exponent notation requires a mandatory '+' or '-' sign immediately after 'e'/'E', followed by at least one digit",
      mark
    );
    return null;
  }
  return { type: "scalar", kind: "bare", value: raw };
}
function parseScalar(source) {
  const ch = source.peek();
  if (ch === '"') return parseQuotedString(source);
  if (ch === "#") return parseHexOrError(source);
  if (ch === "-" || source.isDigit(ch) || source.isIdentifierStart(ch)) return parseBareToken(source);
  source.setError(ErrorCode.EXPECTED_SCALAR, `Expected a scalar value, got '${ch}'`);
  return null;
}
function parseBlobBody(source, tag) {
  source.advance();
  const contentMark = source.mark();
  while (!source.isEof() && source.peek() !== "`") source.advance();
  if (source.isEof()) {
    source.setError(ErrorCode.UNTERMINATED_STRING, "Unterminated blob", contentMark);
    return null;
  }
  const content = source.sliceFrom(contentMark).toString();
  source.advance();
  return { type: "blob", tag: tag || null, content };
}
function parseTaggedBlob(source) {
  source.advance();
  const tag = scanIdentifier(source);
  if (tag === null) {
    source.setError(ErrorCode.EXPECTED_IDENTIFIER, "Expected a blob tag identifier");
    return null;
  }
  if (source.peek() !== "`") {
    source.setError(ErrorCode.UNEXPECTED_TOKEN, "Expected '`' to begin blob content");
    return null;
  }
  return parseBlobBody(source, tag);
}
function parseVarRef(source, doc) {
  const dollarMark = source.mark();
  source.advance();
  if (source.isEof() || !source.isIdentifierStart(source.peek())) {
    source.setError(ErrorCode.MALFORMED_VARREF, "'$' must be followed immediately by an identifier", dollarMark);
    return null;
  }
  const refTarget = scanIdentifier(source);
  if (source.peek() === ".") {
    source.setError(ErrorCode.MALFORMED_VARREF, "Dotted-path VarRefs are not valid in AML", dollarMark);
    return null;
  }
  const node = { type: "scalar", kind: "varref", refTarget };
  doc.varRefs.push(node);
  return node;
}
function parseAmpElement(source, doc) {
  const ch = source.peek();
  if (ch === "{") {
    source.setError(ErrorCode.AMP_OBJECT_FORBIDDEN, "Objects are forbidden in AMP");
    return null;
  }
  if (ch === "[" || ch === "(") {
    source.setError(ErrorCode.AMP_ARRAY_ELEMENT_NOT_SCALAR, "AMP array/tuple elements must be scalar");
    return null;
  }
  if (ch === "$") {
    source.setError(ErrorCode.AMP_VARREF_FORBIDDEN, "'$' VarRef is forbidden in AMP");
    return null;
  }
  if (ch === "`") return parseBlobBody(source, null);
  if (ch === "@") return parseTaggedBlob(source);
  return parseScalar(source);
}
function parseAmpValue(source, doc, dispatchMark) {
  const ch = source.peek();
  if (ch === "{") {
    source.setError(ErrorCode.AMP_OBJECT_FORBIDDEN, "Objects are forbidden in AMP", dispatchMark);
    return null;
  }
  if (ch === "$") {
    source.setError(ErrorCode.AMP_VARREF_FORBIDDEN, "'$' VarRef is forbidden in AMP", dispatchMark);
    return null;
  }
  if (ch === "[") return parseArray(source, DIALECT_AMP, doc);
  if (ch === "(") return parseTuple(source, DIALECT_AMP, doc);
  if (ch === "`") return parseBlobBody(source, null);
  if (ch === "@") return parseTaggedBlob(source);
  return parseScalar(source);
}
function parseAmlValue(source, doc) {
  const ch = source.peek();
  if (ch === "{") return parseObject(source, DIALECT_AML, doc);
  if (ch === "[") return parseArray(source, DIALECT_AML, doc);
  if (ch === "(") return parseTuple(source, DIALECT_AML, doc);
  if (ch === "$") return parseVarRef(source, doc);
  if (ch === "`") return parseBlobBody(source, null);
  if (ch === "@") return parseTaggedBlob(source);
  return parseScalar(source);
}
function parseValue(source, dialect, doc, dispatchMark) {
  return dialect === DIALECT_AMP ? parseAmpValue(source, doc, dispatchMark) : parseAmlValue(source, doc);
}
function parseElement(source, dialect, doc) {
  return dialect === DIALECT_AMP ? parseAmpElement(source, doc) : parseAmlValue(source, doc);
}
function parseArray(source, dialect, doc) {
  source.advance();
  if (!skip(source)) return null;
  if (source.peek() === "]") {
    source.setError(ErrorCode.EMPTY_ARRAY_NOT_ALLOWED, "Empty arrays are not allowed");
    return null;
  }
  const elements = [];
  while (true) {
    const el = parseElement(source, dialect, doc);
    if (el === null) return null;
    elements.push(el);
    if (!skip(source)) return null;
    if (source.peek() === ",") {
      source.advance();
      if (!skip(source)) return null;
      if (source.peek() === "]") break;
      continue;
    }
    break;
  }
  if (source.peek() !== "]") {
    source.setError(ErrorCode.UNEXPECTED_TOKEN, "Expected ',' or ']'");
    return null;
  }
  source.advance();
  return { type: "array", elements };
}
function parseTuple(source, dialect, doc) {
  source.advance();
  if (!skip(source)) return null;
  if (source.peek() === ")") {
    source.setError(ErrorCode.EMPTY_TUPLE_NOT_ALLOWED, "Empty tuples are not allowed");
    return null;
  }
  const elements = [];
  while (true) {
    const el = parseElement(source, dialect, doc);
    if (el === null) return null;
    elements.push(el);
    if (!skip(source)) return null;
    if (source.peek() === ",") {
      source.advance();
      if (!skip(source)) return null;
      if (source.peek() === ")") break;
      continue;
    }
    break;
  }
  if (source.peek() !== ")") {
    source.setError(ErrorCode.UNEXPECTED_TOKEN, "Expected ',' or ')'");
    return null;
  }
  source.advance();
  if (elements.length < 2) {
    source.setError(ErrorCode.TUPLE_TOO_FEW_ELEMENTS, "Tuples require at least two elements");
    return null;
  }
  return { type: "tuple", elements };
}
function scanAttributeScalarText(source) {
  if (source.peek() === '"') {
    const str = parseQuotedString(source);
    return str === null ? null : str.value;
  }
  const mark = source.mark();
  while (!source.isEof() && !source.isWhitespace(source.peek()) && source.peek() !== "," && source.peek() !== "]" && source.peek() !== ";") {
    source.advance();
  }
  if (source.pos === mark.pos) {
    source.setError(ErrorCode.EXPECTED_VALUE, "Expected an attribute value");
    return null;
  }
  return source.sliceFrom(mark).toString();
}
function parseAttributeList(source) {
  source.advance();
  source.advance();
  if (!skip(source)) return null;
  const attrs = [];
  if (source.peek() === "]") {
    source.advance();
    return attrs;
  }
  while (true) {
    const key = scanIdentifier(source);
    if (key === null) {
      source.setError(ErrorCode.EXPECTED_IDENTIFIER, "Expected an attribute name");
      return null;
    }
    if (!skip(source)) return null;
    let value = "";
    if (source.peek() === "=") {
      source.advance();
      if (!skip(source)) return null;
      const text = scanAttributeScalarText(source);
      if (text === null) return null;
      value = text;
    }
    attrs.push({ key, value });
    if (!skip(source)) return null;
    if (source.peek() === ",") {
      source.advance();
      if (!skip(source)) return null;
      continue;
    }
    break;
  }
  if (source.peek() !== "]") {
    source.setError(ErrorCode.UNEXPECTED_TOKEN, "Expected ',' or ']' in attribute list");
    return null;
  }
  source.advance();
  return attrs;
}
function parseField(source, dialect, doc) {
  const name = scanIdentifier(source);
  if (name === null) {
    source.setError(ErrorCode.EXPECTED_IDENTIFIER, "Expected a field name");
    return null;
  }
  if (!skip(source)) return null;
  let attrs = [];
  if (source.peek() === "@" && source.peek(1) === "[") {
    if (dialect === DIALECT_AMP) {
      source.setError(ErrorCode.AMP_ATTRIBUTE_FORBIDDEN, "Attributes are forbidden in AMP");
      return null;
    }
    attrs = parseAttributeList(source);
    if (attrs === null) return null;
    if (!skip(source)) return null;
  }
  if (!source.match(":=")) {
    source.setError(ErrorCode.EXPECTED_ASSIGN, "Expected ':='");
    return null;
  }
  source.consume(2);
  const dispatchMark = source.mark();
  if (!skip(source)) return null;
  const value = parseValue(source, dialect, doc, dispatchMark);
  if (value === null) return null;
  if (!skip(source)) return null;
  if (source.peek() !== ";") {
    source.setError(ErrorCode.MISSING_STATEMENT_TERMINATOR, "Expected ';'", insertionPoint(source));
    return null;
  }
  source.advance();
  value.name = name;
  value.attributes = attrs;
  return { name, value };
}
function parseObject(source, dialect, doc) {
  source.advance();
  if (!skip(source)) return null;
  if (source.peek() === "}") {
    source.setError(ErrorCode.EMPTY_OBJECT_NOT_ALLOWED, "Empty objects are not allowed");
    return null;
  }
  const fields = /* @__PURE__ */ new Map();
  const fieldOrder = [];
  while (source.peek() !== "}") {
    const field = parseField(source, dialect, doc);
    if (field === null) return null;
    fields.set(field.name, field.value);
    fieldOrder.push(field.name);
    if (!skip(source)) return null;
    if (source.peek() === ",") {
      source.advance();
      if (!skip(source)) return null;
    }
    if (source.isEof()) {
      source.setError(ErrorCode.UNEXPECTED_TOKEN, "Expected '}'");
      return null;
    }
  }
  source.advance();
  return { type: "object", fields, fieldOrder };
}
function parseStatement(source, dialect, doc) {
  const name = scanIdentifier(source);
  if (name === null) {
    source.setError(ErrorCode.EXPECTED_IDENTIFIER, "Expected a statement identifier");
    return null;
  }
  if (!skip(source)) return null;
  let base = null;
  if (source.peek() === ":" && source.peek(1) !== "=") {
    source.advance();
    if (!skip(source)) return null;
    base = scanIdentifier(source);
    if (base === null) {
      source.setError(ErrorCode.EXPECTED_IDENTIFIER, "Expected a base identifier");
      return null;
    }
    if (dialect === DIALECT_AMP) {
      source.setError(ErrorCode.AMP_INHERITANCE_FORBIDDEN, "Inheritance is forbidden in AMP");
      return null;
    }
    if (!skip(source)) return null;
  }
  let attrs = [];
  if (source.peek() === "@" && source.peek(1) === "[") {
    if (dialect === DIALECT_AMP) {
      source.setError(ErrorCode.AMP_ATTRIBUTE_FORBIDDEN, "Attributes are forbidden in AMP");
      return null;
    }
    attrs = parseAttributeList(source);
    if (attrs === null) return null;
    if (!skip(source)) return null;
  }
  if (source.match(":=")) {
    source.consume(2);
    const dispatchMark = source.mark();
    if (!skip(source)) return null;
    const value = parseValue(source, dialect, doc, dispatchMark);
    if (value === null) return null;
    if (base !== null && value.type !== "object") {
      source.setError(
        ErrorCode.INHERITANCE_REQUIRES_OBJECT,
        "Inheritance requires an object value \u2014 scalars, arrays, tuples, and blobs are not inheritable",
        dispatchMark
      );
      return null;
    }
    if (!skip(source)) return null;
    if (source.peek() !== ";") {
      source.setError(ErrorCode.MISSING_STATEMENT_TERMINATOR, "Expected ';'", insertionPoint(source));
      return null;
    }
    source.advance();
    value.name = name;
    value.attributes = attrs;
    value.base = base;
    value.moduleAttributes = doc.moduleAttributes;
    value._document = doc;
    return { name, base, attrs, value };
  }
  if (source.peek() === "{") {
    if (dialect === DIALECT_AMP) {
      source.setError(ErrorCode.AMP_OBJECT_FORBIDDEN, "Objects are forbidden in AMP");
      return null;
    }
    const value = parseObject(source, dialect, doc);
    if (value === null) return null;
    if (!skip(source)) return null;
    if (source.peek() !== ";") {
      source.setError(ErrorCode.MISSING_STATEMENT_TERMINATOR, "Expected ';'", insertionPoint(source));
      return null;
    }
    source.advance();
    value.name = name;
    value.attributes = attrs;
    value.base = base;
    value.moduleAttributes = doc.moduleAttributes;
    value._document = doc;
    return { name, base, attrs, value };
  }
  source.setError(ErrorCode.EXPECTED_ASSIGN, "Expected ':=' or '{'");
  return null;
}
function consumeShebang(source) {
  if (source.match(SHEBANG_AML)) {
    source.consume(SHEBANG_AML.length);
    return DIALECT_AML;
  }
  if (source.match(SHEBANG_AMP)) {
    source.consume(SHEBANG_AMP.length);
    return DIALECT_AMP;
  }
  if (source.match(SHEBANG_ASL)) {
    source.consume(SHEBANG_ASL.length);
    return DIALECT_ASL;
  }
  return DIALECT_AML;
}
function parseDocument(source) {
  const dialect = consumeShebang(source);
  const doc = { kind: "document", dialect, moduleAttributes: [], statements: [], varRefs: [], registry: /* @__PURE__ */ new Map() };
  if (!skip(source)) return null;
  if (source.peek() === "@" && source.peek(1) === "[") {
    if (dialect === DIALECT_AMP) {
      source.setError(ErrorCode.AMP_ATTRIBUTE_FORBIDDEN, "Module-level attributes are forbidden in AMP");
      return null;
    }
    const attrs = parseAttributeList(source);
    if (attrs === null) return null;
    doc.moduleAttributes = attrs;
    if (!skip(source)) return null;
  }
  while (!source.isEof()) {
    if (!skip(source)) return null;
    if (source.isEof()) break;
    const stmt = parseStatement(source, dialect, doc);
    if (stmt === null) return null;
    doc.statements.push(stmt);
    if (stmt.name) doc.registry.set(stmt.name, stmt.value);
    if (!skip(source)) return null;
  }
  return doc;
}
var RAW_VALUE_UNSUPPORTED = /* @__PURE__ */ Symbol("parseRawValue: unsupported construct");
function parseRawValueBody(source, dialect, doc) {
  const ch = source.peek();
  if (ch === "[") return parseArray(source, dialect, doc);
  if (ch === "(") return parseTuple(source, dialect, doc);
  if (ch === "{") {
    if (dialect === DIALECT_AMP) {
      source.setError(ErrorCode.AMP_OBJECT_FORBIDDEN, "Objects are forbidden in AMP");
      return null;
    }
    return parseObject(source, dialect, doc);
  }
  if (ch === "`") return parseBlobBody(source, null);
  if (ch === "@") return parseTaggedBlob(source);
  if (ch === "$") {
    source.setError(
      ErrorCode.EXPECTED_SCALAR,
      "'$' is not supported by parseRawValue -- there is no registry here for a VarRef to resolve against"
    );
    return null;
  }
  return parseScalar(source);
}
function toPlainValue(node) {
  if (node.type === "scalar") {
    switch (node.kind) {
      case "int":
      case "float":
      case "bool":
      case "null":
      case "string":
      case "bare":
        return node.value;
      case "hex":
        return node.value;
      // numeric -- the caller's own schema already knows it's a color
      default:
        return RAW_VALUE_UNSUPPORTED;
    }
  }
  if (node.type === "array" || node.type === "tuple") {
    const out = [];
    for (const el of node.elements) {
      const v = toPlainValue(el);
      if (v === RAW_VALUE_UNSUPPORTED) return RAW_VALUE_UNSUPPORTED;
      out.push(v);
    }
    return out;
  }
  if (node.type === "object") {
    const out = {};
    for (const key of node.fieldOrder) {
      const v = toPlainValue(node.fields.get(key));
      if (v === RAW_VALUE_UNSUPPORTED) return RAW_VALUE_UNSUPPORTED;
      out[key] = v;
    }
    return out;
  }
  if (node.type === "blob") {
    return node.content;
  }
  return RAW_VALUE_UNSUPPORTED;
}
function parseRawValueCore(source, dialect) {
  const doc = { varRefs: [], moduleAttributes: [], registry: /* @__PURE__ */ new Map() };
  if (!skip(source)) return void 0;
  let node;
  if (source.isIdentifierStart(source.peek())) {
    const mark = source.mark();
    scanIdentifier(source);
    if (!skip(source)) return void 0;
    if (source.match(":=")) {
      source.consume(2);
      if (!skip(source)) return void 0;
      node = parseRawValueBody(source, dialect, doc);
    } else {
      source.restore(mark);
      source.lastError = null;
      node = parseRawValueBody(source, dialect, doc);
    }
  } else {
    node = parseRawValueBody(source, dialect, doc);
  }
  if (node === null) return void 0;
  if (!skip(source)) return void 0;
  if (source.peek() === ";") source.advance();
  if (!skip(source)) return void 0;
  if (!source.isEof()) {
    source.setError(ErrorCode.UNEXPECTED_TOKEN, "Unexpected trailing content after value");
    return void 0;
  }
  const plain = toPlainValue(node);
  if (plain === RAW_VALUE_UNSUPPORTED) {
    source.setError(
      ErrorCode.EXPECTED_SCALAR,
      "Value contains an unresolved $ reference, which parseRawValue does not support at any depth"
    );
    return void 0;
  }
  return plain;
}

// src/resolver.js
var NULL_SHAPE = { type: "scalar", kind: "null", value: null };
var META_KEYS = ["name", "attributes", "base", "moduleAttributes", "_document"];
function resolveTarget(name, registry, visiting, cache) {
  if (cache.has(name)) return cache.get(name);
  if (visiting.has(name)) return NULL_SHAPE;
  if (!registry.has(name)) return NULL_SHAPE;
  visiting.add(name);
  const val = registry.get(name);
  const result = val.type === "scalar" && val.kind === "varref" ? resolveTarget(val.refTarget, registry, visiting, cache) : val;
  visiting.delete(name);
  cache.set(name, result);
  return result;
}
function applyShape(ref, shape) {
  const preserved = {};
  for (const key of META_KEYS) {
    if (key in ref) preserved[key] = ref[key];
  }
  for (const key of Object.keys(ref)) delete ref[key];
  for (const key of Object.keys(shape)) {
    if (!META_KEYS.includes(key)) ref[key] = shape[key];
  }
  Object.assign(ref, preserved);
}
function resolveVarRefs(registry, varRefs) {
  const cache = /* @__PURE__ */ new Map();
  for (const ref of varRefs) {
    const shape = resolveTarget(ref.refTarget, registry, /* @__PURE__ */ new Set(), cache);
    applyShape(ref, shape);
  }
}

// src/node.js
var NodeType = Object.freeze({
  SCALAR: "scalar",
  OBJECT: "object",
  ARRAY: "array",
  TUPLE: "tuple",
  BLOB: "blob"
});
var ScalarKind = Object.freeze({
  STRING: "string",
  INT: "int",
  FLOAT: "float",
  BOOL: "bool",
  NULL: "null",
  HEX: "hex",
  BARE: "bare",
  VARREF: "varref"
});
var AnvilNode = class _AnvilNode {
  constructor(astNode) {
    this._ast = astNode;
  }
  get type() {
    return this._ast ? this._ast.type : null;
  }
  get kind() {
    if (!this._ast || this._ast.type !== "scalar") return null;
    return this._ast.kind;
  }
  asString() {
    const a = this._ast;
    if (!a) return null;
    if (a.type === "blob") return a.content;
    if (a.type !== "scalar") return null;
    switch (a.kind) {
      case "hex":
        return a.hexString;
      case "bool":
        return a.value ? "true" : "false";
      case "null":
        return null;
      case "int":
      case "float":
        return a.raw !== void 0 ? a.raw : String(a.value);
      default:
        return a.value;
    }
  }
  asInt() {
    const a = this._ast;
    if (!a || a.type !== "scalar") return null;
    if (a.kind === "int" || a.kind === "hex") return a.value;
    if (a.kind === "float") return Math.trunc(a.value);
    return null;
  }
  asFloat() {
    const a = this._ast;
    if (!a || a.type !== "scalar") return null;
    if (a.kind === "float" || a.kind === "int") return a.value;
    return null;
  }
  asBool() {
    const a = this._ast;
    if (!a || a.type !== "scalar" || a.kind !== "bool") return null;
    return a.value;
  }
  isNull() {
    return !!this._ast && this._ast.type === "scalar" && this._ast.kind === "null";
  }
  asBuffer() {
    return this._ast && this._ast.slice ? this._ast.slice : null;
  }
  get(key) {
    const a = this._ast;
    if (!a || a.type !== "object") return null;
    const v = a.fields.get(key);
    return v ? new _AnvilNode(v) : null;
  }
  field(key) {
    return this.get(key);
  }
  has(key) {
    const a = this._ast;
    return !!a && a.type === "object" && a.fields.has(key);
  }
  keys() {
    const a = this._ast;
    if (!a || a.type !== "object") return [];
    return [...a.fieldOrder];
  }
  entries() {
    const a = this._ast;
    if (!a || a.type !== "object") return [];
    return a.fieldOrder.map((k) => [k, new _AnvilNode(a.fields.get(k))]);
  }
  get count() {
    const a = this._ast;
    if (!a) return 0;
    if (a.type === "object") return a.fieldOrder.length;
    if (a.type === "array" || a.type === "tuple") return a.elements.length;
    return 0;
  }
  at(index) {
    const a = this._ast;
    if (!a || a.type !== "array" && a.type !== "tuple") return null;
    const el = a.elements[index];
    return el ? new _AnvilNode(el) : null;
  }
  hasBase() {
    return !!(this._ast && this._ast.base);
  }
  baseIdentifier() {
    return this._ast && this._ast.base || null;
  }
  is(name) {
    return !!(this._ast && this._ast.name === name);
  }
  _attributeList() {
    const a = this._ast;
    if (!a) return [];
    const own = a.attributes || [];
    const mod = a.moduleAttributes || [];
    return [...own, ...mod];
  }
  hasAttribute(key) {
    return this._attributeList().some((a) => a.key === key);
  }
  attribute(key) {
    const found = this._attributeList().find((a) => a.key === key);
    return found ? found.value : null;
  }
  get attributes() {
    return this._attributeList();
  }
  *[Symbol.iterator]() {
    const a = this._ast;
    if (!a) return;
    if (a.type === "array" || a.type === "tuple") {
      for (const el of a.elements) yield new _AnvilNode(el);
    } else if (a.type === "object") {
      for (const k of a.fieldOrder) yield new _AnvilNode(a.fields.get(k));
    }
  }
};

// src/writer.js
var STRUCTURAL_CHARS = /[\s,{}[\]();]/;
function needsQuote(text) {
  if (text === "") return true;
  if (text === "true" || text === "false" || text === "null") return true;
  if (STRUCTURAL_CHARS.test(text)) return true;
  if (text[0] === "`" || text[0] === "@" || text[0] === "$") return true;
  return false;
}
function quote(text) {
  return '"' + text.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\n/g, "\\n").replace(/\t/g, "\\t").replace(/\r/g, "\\r") + '"';
}
function writeScalar(a) {
  switch (a.kind) {
    case "string":
    case "bare": {
      const text = a.value;
      return needsQuote(text) ? quote(text) : text;
    }
    case "int":
    case "float":
      return a.raw !== void 0 ? a.raw : String(a.value);
    case "bool":
      return a.value ? "true" : "false";
    case "null":
      return "null";
    case "hex":
      return "#" + a.hexString;
    case "varref":
      return "$" + a.refTarget;
    default:
      return "";
  }
}
function writeAttributeList(attrs) {
  if (!attrs || attrs.length === 0) return "";
  const parts = attrs.map((a) => a.value ? `${a.key}=${a.value}` : a.key);
  return `@[${parts.join(", ")}]`;
}
function writeValue(a, opts, indent) {
  if (a.type === "blob") {
    const tagPart = a.tag ? "@" + a.tag : "";
    return `${tagPart}\`${a.content}\``;
  }
  if (a.type === "tuple") {
    return `(${a.elements.map((e) => writeValue(e, opts, indent)).join(", ")})`;
  }
  if (a.type === "array") {
    return `[${a.elements.map((e) => writeValue(e, opts, indent)).join(", ")}]`;
  }
  if (a.type === "object") {
    return writeObjectBody(a, opts, indent);
  }
  return writeScalar(a);
}
function writeObjectBody(a, opts, indent) {
  const nextIndent = indent + "    ";
  const parts = a.fieldOrder.map((k) => {
    const fv = a.fields.get(k);
    const attrStr = writeAttributeList(fv.attributes);
    const attrPart = attrStr ? " " + attrStr : "";
    const valueStr = writeValue(fv, opts, nextIndent);
    return opts.minify ? `${k}${attrPart}:=${valueStr};` : `${nextIndent}${k}${attrPart} := ${valueStr};`;
  });
  if (opts.minify) return `{${parts.join("")}}`;
  if (parts.length === 0) return "{}";
  return `{
${parts.join("\n")}
${indent}}`;
}
function writeStatement(stmt, opts) {
  const basePart = stmt.base ? ` : ${stmt.base}` : "";
  const attrStr = writeAttributeList(stmt.attrs);
  const attrPart = attrStr ? " " + attrStr : "";
  const valueStr = writeValue(stmt.value, opts, "");
  return opts.minify ? `${stmt.name}${basePart}${attrPart}:=${valueStr};` : `${stmt.name}${basePart}${attrPart} := ${valueStr};`;
}
function write(node, options = {}) {
  const { minify = false } = options;
  const doc = node && node._ast && node._ast._document;
  if (!doc) return "";
  const shebang = doc.dialect === "amp" ? "#!amp" : doc.dialect === "asl" ? "#!asl" : "#!aml";
  const opts = { minify };
  const stmtStrings = doc.statements.map((s) => writeStatement(s, opts));
  if (minify) {
    return shebang + stmtStrings.join(",");
  }
  return shebang + "\n" + stmtStrings.map((s) => s + "\n").join("");
}

// src/index.js
var _lastError = null;
function makeResult(source, doc) {
  _lastError = source.lastError;
  if (!doc) return null;
  resolveVarRefs(doc.registry, doc.varRefs);
  const fields = /* @__PURE__ */ new Map();
  const fieldOrder = [];
  for (const stmt of doc.statements) {
    fields.set(stmt.name, stmt.value);
    fieldOrder.push(stmt.name);
  }
  const root = {
    type: "object",
    fields,
    fieldOrder,
    attributes: [],
    moduleAttributes: doc.moduleAttributes,
    _document: doc
  };
  return new AnvilNode(root);
}
function parse(sourceText, file = null) {
  _lastError = null;
  const source = new Source(sourceText, file);
  const doc = parseDocument(source);
  return makeResult(source, doc);
}
async function load(path) {
  _lastError = null;
  let fs;
  try {
    fs = await import("node:fs/promises");
  } catch {
    _lastError = new AnvilError(ErrorCode.INVALID_PATH, "File loading requires Node.js");
    return null;
  }
  try {
    const text = await fs.readFile(path, "utf8");
    return parse(text, path);
  } catch (err) {
    _lastError = new AnvilError(
      ErrorCode.FILE_NOT_FOUND,
      err?.message || `Could not read file: ${path}`,
      0,
      0,
      path
    );
    return null;
  }
}
function parseRawValue(sourceText, options = {}) {
  _lastError = null;
  const { dialect = "aml", file = null } = options;
  const source = new Source(sourceText, file);
  const result = parseRawValueCore(source, dialect === "amp" ? DIALECT_AMP : DIALECT_AML);
  _lastError = source.lastError;
  return source.lastError ? null : result;
}
function lastError() {
  return _lastError;
}
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  AnvilError,
  AnvilNode,
  ErrorCode,
  lastError,
  load,
  parse,
  parseRawValue,
  write
});
