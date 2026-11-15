'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {resolveRuntime, discoverCards} = require('../shell/platform.cjs');
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'CrossNext-platform-test-'));
(async () => {
  // Reproduce the user's unsigned Mac layout, including spaces and NFD accents.
  const resources = path.join(root, 'carpeta sin ti\u0301tulo', 'CrossNext Book Preparer.app', 'Contents', 'Resources');
  const appPath = path.join(resources, 'app');
  fs.mkdirSync(path.join(resources, 'runtime'), {recursive: true});
  fs.mkdirSync(path.join(appPath, 'build', 'desktop-runtime'), {recursive: true});
  assert.equal(resolveRuntime(resources, appPath), path.join(resources, 'runtime'));
  assert.equal(resolveRuntime(path.join(root, 'vendor-resources'), appPath), path.join(resources, 'runtime'));
  const incomplete = path.join(root, 'Incomplete.app', 'Contents', 'Resources', 'app');
  assert.equal(resolveRuntime(path.join(root, 'vendor-resources'), incomplete), path.join(path.dirname(incomplete), 'runtime'));
  assert.equal(resolveRuntime(path.join(root, 'vendor-resources'), root), path.join(root, 'build', 'desktop-runtime'));

  const volumes = path.join(root, 'Volumes');
  const card = path.join(volumes, 'MICRO SD 日本語');
  fs.mkdirSync(path.join(card, '.crosspoint'), {recursive: true});
  fs.writeFileSync(path.join(card, '.crosspoint', 'settings.json'), '{"fontSize":14}');
  fs.mkdirSync(path.join(volumes, 'Unrelated drive'));
  assert.deepEqual(await discoverCards('darwin', volumes), [card]);
  assert.deepEqual(await discoverCards('win32', undefined, [card, path.join(volumes, 'Unrelated drive')]), [card]);
  const second = path.join(volumes, 'SECOND CARD');
  fs.mkdirSync(path.join(second, '.crosspoint'), {recursive: true});
  assert.equal((await discoverCards('darwin', volumes)).length, 2);
  assert.equal(fs.readFileSync(path.join(card, '.crosspoint', 'settings.json'), 'utf8'), '{"fontSize":14}');
  fs.renameSync(card, path.join(root, 'Detached card'));
  assert.deepEqual(await discoverCards('darwin', volumes), [second]);
  assert.deepEqual(await discoverCards('darwin', path.join(root, 'missing')), []);
  console.log('PASS Mac bundled/development runtime paths, NFD accents, mounted cards, ambiguity, removal and preserved settings');
})().catch(error => {console.error(error);process.exitCode = 1;}).finally(() => {
  fs.rmSync(root, {recursive: true, force: true});
});
