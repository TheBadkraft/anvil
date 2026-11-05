// v0.14.0-rc probe for the INSTALLED anvil.js package. Run from a directory with node_modules/anvil.js; prints PASS/FAIL lines.
// argv[2] is a scratch directory (becomes the cwd, so includes resolve against it).
const assert = require('assert');
const fs = require('fs'), path = require('path');
const a = require('anvil.js');
const scratch = process.argv[2]; fs.mkdirSync(path.join(scratch, 'inc'), { recursive: true }); process.chdir(scratch);
let bad = 0;
const t = (name, fn) => { try { fn(); console.log('PASS js-v014: ' + name); } catch (e) { bad = 1; console.log('FAIL js-v014: ' + name + ': ' + e.message); } };
const code = (fn) => { try { fn(); return 0; } catch (e) { return e.code; } };
const W = a.WriterErrorCode, D = a.AnvilDialect;
t('s1 := { f3 : s1 { a := 1; }; }; is 4022 at once (no endless walk)', () => {
  assert.ok(a.parse('#!aml\ns1 := { f3 : s1 { a := 1; }; };\n') === null, 'the document must not load');
  const e = a.lastError(); assert.strictEqual(e.code, 4022); assert.strictEqual(e.name, 'ANVL_ERR_PARSER_NESTED_BASE');
});
t('a top-level base still loads', () => assert.notStrictEqual(a.parse('#!aml\nb := { a := 1; };\nd : b := { c := 2; };\n'), null));
t('writer: a base inside an object is code 23', () => {
  assert.strictEqual(W.NESTED_BASE, 23);
  assert.strictEqual(code(() => a.AnvilWriter.create(D.AML).statement('o').beginObject().statement('x', 'b')), 23);
});
t('builder: setBase on a nested member is code 23 at once', () => {
  const b = a.AnvilBuilder.create(D.AML); const o = b.object(); const m = o.add('f', b.value(1)); b.add('o', o);
  assert.strictEqual(code(() => m.setBase('o')), 23);
});
t('statementBlock emits name { ... }; and it reloads; scalar value is 21; base naming a block statement is 22', () => {
  const w = a.AnvilWriter.create(D.AML);
  const text = w.statementBlock('ro').beginObject().statement('a').value(1).endObject().finish();
  assert.strictEqual(text, '#!aml\n\nro {\n   a := 1;\n};\n'); assert.notStrictEqual(a.parse(text), null);
  assert.strictEqual(code(() => a.AnvilWriter.create(D.AML).statementBlock('ro').value(1)), 21);
  assert.strictEqual(code(() => a.AnvilWriter.create(D.AML).statementBlock('ro').beginObject().statement('a').value(1).endObject().statement('d', 'ro')), 22);
});
t('setBlock emits the block form; non-object is 21; a base naming it is 22 at emit', () => {
  const b = a.AnvilBuilder.create(D.AML); const o = b.object(); o.add('a', b.value(1)); const m = b.add('ro', o); m.setBlock(true);
  assert.strictEqual(b.emit(), '#!aml\n\nro {\n   a := 1;\n};\n');
  const s = a.AnvilBuilder.create(D.AML); const sm = s.add('s', s.value(1));
  assert.strictEqual(code(() => { sm.setBlock(true); s.emit(); }), 21);
  const c = a.AnvilBuilder.create(D.AML); const o1 = c.object(); o1.add('a', c.value(1)); c.add('ro', o1).setBlock(true);
  const o2 = c.object(); o2.add('c', c.value(2)); c.add('d', o2).setBase('ro');
  assert.strictEqual(code(() => c.emit()), 22);
});
t('lastError().source names the included file; the include error is at the include keyword', () => {
  fs.writeFileSync('inc/a.anvl', '#!aml\n\n  include "missing.anvl";\n');
  assert.strictEqual(a.parse('#!aml\ninclude "inc/a.anvl";\n'), null);
  const e = a.lastError(); assert.strictEqual(e.code, 4204);
  assert.ok(e.source.endsWith('inc/a.anvl'), 'source ' + e.source); assert.strictEqual(e.line, 3); assert.strictEqual(e.column, 3);
  assert.strictEqual(a.parse('#!aml\n\n    include "gone.anvl";\nx := 1;\n'), null);
  const r = a.lastError(); assert.strictEqual(r.source, ''); assert.strictEqual(r.line, 3); assert.strictEqual(r.column, 5);
});
process.exit(bad);
