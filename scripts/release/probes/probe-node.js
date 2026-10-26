'use strict';
// Release probe for anvil-node: argv[2] = extracted package dir.
const path = require('path');
const anvil = require(path.resolve(process.argv[2]));
console.log(`version=${anvil.getVersion()}`);
const inc = anvil.parse('#!aml\ninclude "missing.anvl";\nx := 1;\n');
const e = anvil.lastError();
console.log(`include=${inc === null ? (e && e.message) || '<null, no error>' : '<parsed without error>'}`);
const doc = anvil.parse('#!aml\nname := "David";\n');
let name = '<none>';
if (doc) { const v = doc.name !== undefined ? doc.name : (doc.get && doc.get('name')); name = String(v && v.asString ? v.asString() : v); }
console.log(`name=${name}`);
