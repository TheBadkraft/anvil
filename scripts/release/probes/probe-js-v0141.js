// v0.14.1-rc probe for the INSTALLED anvil.js package. Run from a directory with node_modules/anvil.js; prints PASS/FAIL lines.
const assert = require('assert');
const a = require('anvil.js');
let bad = 0;
const t = (name, fn) => { try { fn(); console.log('PASS js-v0141: ' + name); } catch (e) { bad = 1; console.log('FAIL js-v0141: ' + name + ': ' + e.message); } };
const code = (fn) => { try { fn(); return 0; } catch (e) { return e.code; } };
const D = a.AnvilDialect;
const refused = (src, pos) => {
  assert.strictEqual(a.parse(src), null, 'must not load');
  const e = a.lastError(); assert.strictEqual(e.code, 4054); assert.strictEqual(e.name, 'ANVL_ERR_RESOLVER_REFERENCE_CYCLE');
  assert.strictEqual(e.line + ':' + e.column, pos);
};
t('s1 := { a := $s1; }; is 4054 at 2:14', () => refused('#!aml\ns1 := { a := $s1; };\n', '2:14'));
t('a nested reference is 4054 at 2:21', () => refused('#!aml\ns1 := { a := { b := $s1; }; };\n', '2:21'));
t('an array holding a reference to itself is 4054 at 2:8', () => refused('#!aml\na := [ $a ];\n', '2:8'));
t('two objects that refer to each other: 4054 at the first reference, 2:13', () => refused('#!aml\na := { x := $b; };\nb := { y := $a; };\n', '2:13'));
t('a cycle closed through inheritance is 4054 at 2:13', () => refused('#!aml\na := { x := $b; };\nb : a := { z := 1; };\n', '2:13'));
t('a reference to a scalar loads', () => assert.notStrictEqual(a.parse('#!aml\nv := 1;\ns1 := { a := $v; };\n'), null));
t('the plain chain cycle a := $b; b := $a; still loads', () => assert.notStrictEqual(a.parse('#!aml\na := $b;\nb := $a;\n'), null));
const cyc = () => { const b = a.AnvilBuilder.create(D.AML); const o = b.object(); o.add('a', b.varref('s1')); b.add('s1', o); return b; };
t('builder: emit refuses the graph with code 24', () => { assert.strictEqual(a.WriterErrorCode.REFERENCE_CYCLE, 24); assert.strictEqual(code(() => cyc().emit()), 24); });
t('builder: write refuses the graph with code 24', () => assert.strictEqual(code(() => cyc().write(a.AnvilWriter.create(D.AML))), 24));
t('builder: a reference to a scalar emits', () => {
  const b = a.AnvilBuilder.create(D.AML); b.add('v', b.value(1)); const o = b.object(); o.add('a', b.varref('v')); b.add('s1', o);
  assert.ok(b.emit().includes('$v'));
});
process.exit(bad);
