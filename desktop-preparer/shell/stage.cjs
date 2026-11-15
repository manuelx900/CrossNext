'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.join(__dirname, '..');
const from = path.join(root, 'dist', 'runtime');
const target = path.join(root, 'build', 'desktop-runtime');
fs.mkdirSync(target, {recursive: true});
const names = fs.readdirSync(from).filter(name => /^(engine\.(cjs|wasm)|engine-build\.json)$|LICENSE|NOTICES/i.test(name));
for (const name of ['engine.cjs', 'engine.wasm', 'engine-build.json']) {
  if (!names.includes(name)) throw Error('Build the firmware engine first: missing ' + name);
}
for (const name of names) fs.copyFileSync(path.join(from, name), path.join(target, name));
fs.copyFileSync(path.join(root, 'src', 'worker.cjs'), path.join(target, 'worker.cjs'));
fs.copyFileSync(path.join(__dirname, 'job.cjs'), path.join(target, 'job.cjs'));
const files = Object.fromEntries(fs.readdirSync(target).filter(name => name !== 'manifest.json').map(name =>
  [name, crypto.createHash('sha256').update(fs.readFileSync(path.join(target, name))).digest('hex')]));
fs.writeFileSync(path.join(target, 'manifest.json'), JSON.stringify({product: 'CrossNext Book Preparer', engine: 'firmware WebAssembly', files}, null, 2));
console.log('Staged firmware engine and licenses; Electron supplies the platform Node runtime.');
