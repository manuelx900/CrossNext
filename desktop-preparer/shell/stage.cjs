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
const version = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8')).version;
const engine = JSON.parse(fs.readFileSync(path.join(from, 'engine-build.json'), 'utf8'));
const firmware = fs.readFileSync(path.join(root, '..', 'platformio.ini'), 'utf8').match(/\[crossplay\]\s+version\s*=\s*([^\s]+)/)?.[1];
const cacheVersion = Number(fs.readFileSync(path.join(root, '..', 'lib/Epub/Epub/Section.cpp'), 'utf8').match(/SECTION_FILE_VERSION = (\d+)/)?.[1]);
const fontFlags = [...fs.readFileSync(path.join(root, '..', 'platformio.ini'), 'utf8').matchAll(/^\s*-D(FREEINK_FONT_ENABLE_[A-Z_]+)=([01])\s*$/gm)];
for (const [, name, value] of fontFlags)
  if (engine.fontFeatures?.[name] !== Number(value)) throw Error('Rebuild the preparation engine with matching firmware font features: ' + name);
if (engine.applicationVersion !== version || engine.firmwareVersion !== firmware || engine.cacheVersion !== cacheVersion) {
  throw Error('Build the preparation engine for this app, firmware and cache version before packaging.');
}
for (const name of names) fs.copyFileSync(path.join(from, name), path.join(target, name));
fs.copyFileSync(path.join(root, 'src', 'worker.cjs'), path.join(target, 'worker.cjs'));
fs.copyFileSync(path.join(__dirname, 'card-storage.cjs'), path.join(target, 'card-storage.cjs'));
fs.copyFileSync(path.join(__dirname, 'cache-transaction.cjs'), path.join(target, 'cache-transaction.cjs'));
fs.copyFileSync(path.join(__dirname, 'job.cjs'), path.join(target, 'job.cjs'));
const files = Object.fromEntries(fs.readdirSync(target).filter(name => name !== 'manifest.json').map(name =>
  [name, crypto.createHash('sha256').update(fs.readFileSync(path.join(target, name))).digest('hex')]));
fs.writeFileSync(path.join(target, 'manifest.json'), JSON.stringify({product: 'CrossNext Book Preparer', engine: 'firmware WebAssembly', files}, null, 2));
console.log('Staged firmware engine and licenses; Electron supplies the platform Node runtime.');
