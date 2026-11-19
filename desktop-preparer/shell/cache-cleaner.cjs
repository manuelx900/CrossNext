'use strict';
// Preview and remove only recognized, regenerable reader cache files.
const fs = require('node:fs');
const path = require('node:path');
const readline = require('node:readline');
const {once} = require('node:events');
const {safeDirectory, rejectLinks, spaceInfo} = require('./card-storage.cjs');
const defer = () => new Promise(resolve => setImmediate(resolve));
const bookKey = /^(?:epub|xtc)_(?:0|[1-9][0-9]{0,9})$/;
function cacheKind(relative) {
  const interrupted = '.crossnext-preparing';
  if (relative.endsWith(interrupted)) {
    const canonical = relative.slice(0, -interrupted.length);
    if (canonical.endsWith(interrupted)) return null;
    return cacheKind(canonical) ? 'interrupted' : null;
  }
  const parts = relative.split('/');
  if (parts.some(p => !p || p === '.' || p === '..' || p.includes('\\') || p.includes(':'))) return null;
  if (parts.length === 1 && /^library-grid-[0-3]\.pack$/.test(parts[0])) return 'shared';
  if (!bookKey.test(parts[0]) || Number(parts[0].split('_')[1]) > 0xffffffff) return null;
  let name = parts.at(-1);
  if (name.startsWith('._')) name = name.slice(2);
  if (parts.length === 2) {
    if (/^ready-v1-[a-f0-9]{8}\.bin$/.test(name)) return 'stamp';
    if (/^(?:book\.bin|\.items\.bin|css_rules\.cache|\.desktop-source-v1\.bin|\.desktop-ready-v2\.json|cover(?:_ccw3)?\.bmp|thumb_(?:ccw3_)?[0-9]+\.bmp)$/.test(name) ||
        /\.pxc$|\.css\.bin$/.test(name) || /^\.css[-_.a-f0-9]*$/.test(name)) return 'shared';
  }
  if (parts.length !== 3) return null;
  if (parts[1] === 'sections') {
    if (/^ready-v1-[a-f0-9]{8}\.bin$/.test(name)) return 'stamp';
    if (/^[0-9]+\.(?:classic-)?p[a-f0-9]{8}\.bin(?:\.(?:part|swap|tmp|prep\.tmp))?$/.test(name)) return 'prepared';
    if (/^[0-9]+(?:\.classic)?\.bin(?:\.[1-3])?(?:\.(?:part|swap|tmp))?$/.test(name)) return 'shared';
  }
  if (parts[1] === 'html' && /^(?:\.tmp_)?[0-9]+\.html$/.test(name)) return 'shared';
  if (parts[1] === 'events-v1' && /^[0-9]+\.evc(?:\.tmp)?$/.test(name)) return 'shared';
  return null;
}
function day(value) {
  if (typeof value !== 'string' || !/^\d{4}-\d\d-\d\d$/.test(value)) throw Error('Choose valid start and end dates.');
  const [year, month, date] = value.split('-').map(Number);
  const result = new Date(year, month - 1, date);
  if (result.getFullYear() !== year || result.getMonth() !== month - 1 || result.getDate() !== date) throw Error('Choose valid start and end dates.');
  return result;
}
function period(filter) {
  if (filter?.mode === 'all') return null;
  if (filter?.mode !== 'period') throw Error('Choose a cache cleanup mode.');
  const from = day(filter.from), until = day(filter.to);
  until.setDate(until.getDate() + 1);
  if (from >= until) throw Error('The end date must not precede the start date.');
  return {from: from.getTime(), until: until.getTime()};
}
const identity = stat => ({dev: String(stat.dev), ino: String(stat.ino), size: String(stat.size),
  modified: String(stat.mtimeNs), changed: String(stat.ctimeNs)});
const sameIdentity = (a, b) => Object.keys(a).every(key => a[key] === b[key]);
function directoryIdentity(directory) {
  const stat = fs.lstatSync(directory, {bigint: true});
  if (!stat.isDirectory() || stat.isSymbolicLink()) throw Error('The selected cache directory changed. Choose the card again.');
  return {dev: String(stat.dev), ino: String(stat.ino)};
}
async function *entries(directory) {
  try {
    const stream = await fs.promises.opendir(directory);
    for await (const entry of stream) yield entry;
  } catch (error) { if (error.code !== 'ENOENT') throw error; }
}
async function *bookFiles(root, key) {
  const directory = rejectLinks(root, '.crosspoint/' + key);
  for await (const entry of entries(directory)) {
    if (entry.isSymbolicLink()) continue;
    if (entry.isFile()) yield key + '/' + entry.name;
    if (entry.isDirectory() && ['sections', 'html', 'events-v1'].includes(entry.name))
      for await (const child of entries(path.join(directory, entry.name)))
        if (child.isFile() && !child.isSymbolicLink()) yield key + '/' + entry.name + '/' + child.name;
  }
}
async function scanCachePlan(card, filter, filename, {signal, onProgress = () => {}} = {}) {
  const root = safeDirectory(card), range = period(filter), space = spaceInfo(root);
  const cache = rejectLinks(root, '.crosspoint');
  const plan = {root, filter, filename, rootIdentity: directoryIdentity(root),
    cacheIdentity: fs.existsSync(cache) ? directoryIdentity(cache) : null,
    files: 0, logicalBytes: 0, allocatedBytes: 0, validationFiles: 0, scanned: 0, ...space};
  const output = fs.createWriteStream(filename, {flags: 'wx'});
  const failure = new Promise((_, reject) => output.once('error', reject));
  failure.catch(() => {});
  const record = async (relative, dependency = false) => {
    const kind = cacheKind(relative);
    if (!kind) return false;
    const absolute = rejectLinks(root, '.crosspoint/' + relative);
    let stat;
    try { stat = fs.lstatSync(absolute, {bigint: true}); } catch (error) { if (error.code === 'ENOENT') return false; throw error; }
    if (!stat.isFile() || stat.isSymbolicLink()) return false;
    const modified = Number(stat.mtimeNs / 1000000n);
    if (range && !dependency && (kind !== 'prepared' || modified < range.from || modified >= range.until)) return false;
    const size = Number(stat.size);
    const data = JSON.stringify({relative, dependency, identity: identity(stat)}) + '\n';
    if (!output.write(data)) await Promise.race([once(output, 'drain'), failure]);
    ++plan.files; plan.logicalBytes += size;
    plan.allocatedBytes += Math.ceil(size / space.blockBytes) * space.blockBytes;
    if (dependency) ++plan.validationFiles;
    return true;
  };
  try {
    for await (const entry of entries(cache)) {
      if (signal?.aborted) throw Object.assign(Error('Cleanup cancelled.'), {name: 'AbortError'});
      if (entry.isSymbolicLink()) continue;
      if (entry.isFile()) { if (!range) await record(entry.name); continue; }
      if (!entry.isDirectory() || !bookKey.test(entry.name)) continue;
      let selected = !range;
      if (range)
        for await (const relative of bookFiles(root, entry.name)) {
          if (signal?.aborted) throw Object.assign(Error('Cleanup cancelled.'), {name: 'AbortError'});
          if (cacheKind(relative) !== 'prepared') continue;
          let stat;
          try { stat = fs.lstatSync(rejectLinks(root, '.crosspoint/' + relative)); }
          catch (error) { if (error.code === 'ENOENT') continue; throw error; }
          if (stat.mtimeMs >= range.from && stat.mtimeMs < range.until) { selected = true; break; }
        }
      if (!selected) continue;
      // Remove validation stamps before pages, including when cleanup is cancelled.
      for await (const relative of bookFiles(root, entry.name))
        if (cacheKind(relative) === 'stamp') await record(relative, true);
      for await (const relative of bookFiles(root, entry.name)) {
        if (signal?.aborted) throw Object.assign(Error('Cleanup cancelled.'), {name: 'AbortError'});
        if (cacheKind(relative) !== 'stamp') await record(relative);
        if (++plan.scanned % 256 === 0) { onProgress({scanned: plan.scanned, files: plan.files}); await defer(); }
      }
    }
    output.end();
    await Promise.race([once(output, 'finish'), failure]);
    if (!output.closed) await new Promise(resolve => output.once('close', resolve));
    return plan;
  } catch (error) {
    output.destroy();
    if (!output.closed) await new Promise(resolve => output.once('close', resolve));
    throw error;
  }
}
async function deleteCachePlan(plan, {signal, onProgress = () => {}} = {}) {
  const root = safeDirectory(plan.root), range = period(plan.filter);
  if (!sameIdentity(plan.rootIdentity, directoryIdentity(root)) ||
      (plan.cacheIdentity && !sameIdentity(plan.cacheIdentity, directoryIdentity(rejectLinks(root, '.crosspoint')))))
    throw Error('The card changed. Preview the cache again.');
  const result = {deleted: 0, skipped: 0, failed: 0, freedBytes: 0, cancelled: false};
  const input = fs.createReadStream(plan.filename);
  const lines = readline.createInterface({input, crlfDelay: Infinity});
  let visited = 0, blockedBook = null;
  try {
    for await (const line of lines) {
      if (++visited % 128 === 0) { onProgress(result); await defer(); }
      if (signal?.aborted) { result.cancelled = true; break; }
      const item = JSON.parse(line), kind = cacheKind(item.relative);
      if (!kind || (range && kind !== 'prepared' && !(kind === 'stamp' && item.dependency))) throw Error('Unsafe cache cleanup plan. Preview again.');
      const key = item.relative.split('/')[0];
      if (key === blockedBook) { ++result.skipped; continue; }
      const absolute = rejectLinks(root, '.crosspoint/' + item.relative);
      try {
        const stat = fs.lstatSync(absolute, {bigint: true});
        if (!stat.isFile() || stat.isSymbolicLink() || !sameIdentity(item.identity, identity(stat))) {
          if (kind === 'stamp') blockedBook = key;
          ++result.skipped; continue;
        }
        const modified = Number(stat.mtimeNs / 1000000n);
        if (range && !item.dependency && (modified < range.from || modified >= range.until)) throw Error('Cache date changed. Preview again.');
        fs.unlinkSync(absolute);
        ++result.deleted;
        result.freedBytes += Math.ceil(Number(stat.size) / plan.blockBytes) * plan.blockBytes;
      } catch (error) {
        if (error.code === 'ENOENT') ++result.skipped;
        else if (['EACCES', 'EPERM', 'EBUSY'].includes(error.code)) {
          if (kind === 'stamp') blockedBook = key;
          ++result.failed;
        }
        else throw error;
      }
    }
  } finally {
    lines.close(); input.destroy();
    if (!input.closed) await new Promise(resolve => input.once('close', resolve));
  }
  return result;
}
module.exports = {cacheKind, period, scanCachePlan, deleteCachePlan};
