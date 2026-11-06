'use strict';
// node:test reporter for check-tdd: one JSON object per finished test, on stdout.
// Tests are reported children first, then their parent; check-tdd rebuilds the tree from `nesting`.
// The error kind comes from the original error (`details.error.cause`), because node wraps every
// failure in an ERR_TEST_FAILURE: an AssertionError has code ERR_ASSERTION, a TypeError has name TypeError.
module.exports = async function* (source) {
   for await (const ev of source) {
      if (ev.type !== 'test:pass' && ev.type !== 'test:fail') continue;
      const d = ev.data;
      const rec = { r: ev.type === 'test:pass' ? 'pass' : 'fail', name: d.name, nesting: d.nesting, file: d.file || null };
      if (d.skip !== undefined && d.skip !== false) rec.r = 'skip';
      if (d.todo !== undefined && d.todo !== false) rec.todo = true;
      if (ev.type === 'test:fail') {
         const e = d.details && d.details.error;
         const c = (e && e.cause) || e || {};
         rec.failureType = (e && e.failureType) || null;
         rec.code = c.code || null;
         rec.errName = c.name || (c.constructor && c.constructor.name) || null;
         rec.message = String(c.message || (e && e.message) || '').split('\n')[0].slice(0, 200);
      }
      yield JSON.stringify(rec) + '\n';
   }
};
