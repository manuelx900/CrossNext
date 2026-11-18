'use strict';
// Only generated cache files are ever committed to the selected card.
const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');
const createEngine = require('./engine.cjs');
const {safeDirectory, rejectLinks, requireSpace} = require('./card-storage.cjs');
const emit = event => process.stdout.write(JSON.stringify(event) + '\n');
const sha = value => crypto.createHash('sha256').update(value).digest('hex');
const defer = () => new Promise(resolve => setImmediate(resolve));
const excluded = /^(\.|languages$|fonts$|sleep$|wallpapers?$|pet$|dictionaries$|System Volume Information$|\$RECYCLE.BIN$)/i;

function scan(root, formats) {
  const books = [], pending = [root];
  while (pending.length) {
    const directory = pending.pop();
    for (const item of fs.readdirSync(directory, {withFileTypes: true})) {
      if (item.isSymbolicLink() || excluded.test(item.name)) continue;
      const absolute = path.join(directory, item.name);
      if (item.isDirectory()) pending.push(absolute);
      else if (item.isFile() && formats.includes(path.extname(item.name).slice(1).toLowerCase())) {
        const relative = path.relative(root, absolute).split(path.sep).join('/');
        if (Buffer.byteLength(relative, 'utf8') > 1000) continue;
        books.push('/' + relative);
      }
    }
  }
  return books.sort();
}
function cacheFile(relative) {
  const parts = relative.replace(/\\/g, '/').split('/');
  const name = parts[parts.length - 1];
  if (parts.includes('..') || /\.tmp$|\.bak$|\.part$/.test(name)) return false;
  return /^(\.desktop-source-v1\.bin|book\.bin|\.items\.bin|css_rules\.cache|cover(?:_ccw3)?\.bmp|thumb_(?:ccw3_)?\d+\.bmp|ready-v1-[a-f0-9]+\.bin|.*\.pxc|.*\.css\.bin)$/.test(name)
    || (parts[0] === 'html' && /^\d+\.html$/.test(name))
    || (parts[0] === 'sections' && /^(?:\d+.*\.bin|ready-v1-[a-f0-9]+\.bin)$/.test(name))
    || /^\.css/.test(name) && !/tmp|bak/.test(name);
}
function sourceIdentity(stat) {
  const date = new Date(stat.mtimeMs), year = date.getFullYear();
  const fatDate = ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate();
  const fatTime = (date.getHours() << 11) | (date.getMinutes() << 5) | Math.floor(date.getSeconds() / 2);
  const identity = Buffer.alloc(12);
  identity.writeBigUInt64LE(BigInt(stat.size), 0);
  identity.writeUInt32LE(((fatDate << 16) | fatTime) >>> 0, 8);
  return identity;
}
function nativeCoverReady(root, spec, rotated) {
  const filename = rejectLinks(root, 'thumb_' + (rotated ? 'ccw3_' : '') + spec.height + '.bmp');
  try {
    const size = fs.statSync(filename).size;
    if (!size) return true; // The firmware uses an empty placeholder when no cover exists.
    if (size < 54 + spec.bytes + 40 || size > 128 * 1024 + 16384 + 40) return false;
    const data = fs.readFileSync(filename), footer = data.subarray(-40);
    if (data.toString('ascii', 0, 2) !== 'BM' || !footer.subarray(0, 8).equals(Buffer.from('CNAT001\0')) ||
        footer.readBigUInt64LE(8) !== BigInt('0x' + spec.key) || footer.readUInt32LE(20) !== spec.bytes) return false;
    const source = footer.readUInt32LE(16);
    if (source !== data.readUInt32LE(2) || source > 128 * 1024 || source + spec.bytes + 40 !== size) return false;
    const checksum = bytes => {
      let hash = 2166136261;
      for (const byte of bytes) hash = Math.imul(hash ^ byte, 16777619) >>> 0;
      return hash;
    };
    return checksum(footer.subarray(0, 36)) === footer.readUInt32LE(36) &&
      checksum(data.subarray(0, source)) === footer.readUInt32LE(28) &&
      checksum(data.subarray(source, size - 40)) === footer.readUInt32LE(24);
  } catch (error) { if (error.code === 'ENOENT') return false; throw error; }
}
function walk(directory, prefix = '') {
  if (!fs.existsSync(directory)) return [];
  const files = [];
  for (const entry of fs.readdirSync(directory, {withFileTypes: true})) {
    if (entry.isSymbolicLink()) throw Error('Linked cache files are not supported.');
    const relative = prefix ? prefix + '/' + entry.name : entry.name;
    if (entry.isDirectory()) files.push(...walk(path.join(directory, entry.name), relative));
    else if (entry.isFile()) files.push(relative);
  }
  return files;
}
function atomicCopy(source, root, relative) {
  const target = rejectLinks(root, relative);
  if (fs.existsSync(target) && fs.statSync(source).size === fs.statSync(target).size) {
    const left = fs.openSync(source, 'r'), right = fs.openSync(target, 'r');
    const a = Buffer.alloc(65536), b = Buffer.alloc(65536);
    let same = true;
    try {
      for (;;) {
        const count = fs.readSync(left, a, 0, a.length, null);
        if (!count) break;
        if (fs.readSync(right, b, 0, count, null) !== count || !a.subarray(0,count).equals(b.subarray(0,count))) { same = false; break; }
      }
    } finally { fs.closeSync(left); fs.closeSync(right); }
    if (same) return;
  }
  fs.mkdirSync(path.dirname(target), {recursive: true});
  requireSpace(root, fs.statSync(source).size);
  const temporary = target + '.crossnext-preparing';
  rejectLinks(root, relative + '.crossnext-preparing');
  try {
    fs.copyFileSync(source, temporary);
    const descriptor = fs.openSync(temporary, 'r+');
    try { fs.fsyncSync(descriptor); } finally { fs.closeSync(descriptor); }
    fs.renameSync(temporary, target);
  } finally {
    if (fs.existsSync(temporary)) fs.unlinkSync(temporary);
  }
}
function profileFor(card, overrides) {
  let saved = {};
  const filename = path.join(card, '.crosspoint', 'settings.json');
  if (fs.existsSync(filename)) {
    if (fs.statSync(filename).size > 1024 * 1024) throw Error('Settings file is too large.');
    saved = JSON.parse(fs.readFileSync(filename, 'utf8'));
  }
  const profile = {...saved, ...overrides};
  if (Object.hasOwn(overrides, 'epubEngine') && ![0, 1].includes(overrides.epubEngine))
    throw Error('Select Modern or Classic EPUB engine.');
  profile.epubEngine = profile.epubEngine === 1 ? 1 : 0;
  // A fresh reader keeps constructor defaults; only an existing file is migrated.
  if (!fs.existsSync(filename) && !Object.hasOwn(profile, 'paragraphIndentSpaces')) profile.paragraphIndentSpaces = 2;
  const storedSize = profile.fontPointSize ?? profile.fontSize ?? 14;
  profile.fontPointSize = storedSize <= 3 ? 12 + storedSize * 2 : storedSize;
  if (!Number.isInteger(profile.fontPointSize) || profile.fontPointSize < 4 || profile.fontPointSize > 255)
    throw Error('Invalid saved reader font size.');
  if (!Object.hasOwn(profile, 'sdFontFamilyName')) profile.sdFontFamilyName = '';
  if (!profile.sdFontFamilyName && profile.fontFamily === 2) profile.sdFontFamilyName = 'OpenDyslexic';
  if (!profile.sdFontFamilyName && (profile.readerFontStorageVersion ?? 0) < 1 && (profile.fontFamily ?? 0) === 0)
    profile.sdFontFamilyName = 'Noto Serif';
  profile.lineSpacing = profile.lineHeight ?? (profile.lineSpacing < 4 ? profile.lineSpacing + 1 : 2);
  profile.extraParagraphSpacing = profile.extraParagraphSpacing ?? profile.paragraphSpacing;
  return profile;
}
function buildLayouts(catalog, configuration, selectedFont, readerPointSize) {
  const layouts = [];
  for (const name of new Set(configuration.fonts || [selectedFont])) {
    const family = catalog.fonts.find(f => f.name === name);
    if (!family || !family.sizes.length) throw Error('Font is not installed on this card: ' + name);
    const nearest = family.sizes.reduce((a,b) => Math.abs(b - readerPointSize) < Math.abs(a - readerPointSize) ? b : a);
    const points = configuration.sizes === 'all' ? family.sizes :
      Array.isArray(configuration.sizes) ? configuration.sizes : [nearest];
    for (const size of new Set(points)) {
      if (!Number.isInteger(size) || !family.sizes.includes(size)) throw Error(name + ': unsupported size ' + size);
      layouts.push({name, size});
    }
  }
  if (!layouts.length) throw Error('Select at least one font and size.');
  return layouts;
}
async function closeEngine(engine) {
  const pool = engine.PThread;
  if (!pool) return;
  const workers = new Set([...pool.unusedWorkers, ...Object.values(pool.pthreads)]);
  const exited = [...workers].map(worker => worker.threadId === -1 ? Promise.resolve() :
    new Promise(resolve => worker.once('exit', resolve)));
  pool.terminateAllThreads();
  await Promise.all(exited);
}
async function main(configuration, mode) {
  const card = safeDirectory(configuration.card);
  const profile = profileFor(card, configuration.profile || {});
  const stage = fs.mkdtempSync(path.join(os.tmpdir(), 'CrossNext-prepare-'));
  const stageCache = path.join(stage, 'cache'); fs.mkdirSync(stageCache);
  const diagnostics = [];
  const errors = [];
  const stderr = line => {
    if (!line) return;
    diagnostics.push(line);
    if (diagnostics.length > 40) diagnostics.shift();
    if (/\[ERR\]|\[WRN\]/.test(line)) { errors.push(line); if (errors.length > 40) errors.shift(); }
  };
  let engine;
  try {
    engine = await createEngine({
      print: stderr, printErr: stderr,
      preRun(module) {
        module.ENV.CROSSPOINT_SIM_SD = '/sd';
        const FS = module.FS, nodefs = FS.filesystems.NODEFS;
        FS.mkdir('/sd'); FS.mount(nodefs, {root: card}, '/sd');
        // The mountpoint exists on a normal reader card; do not create it via NODEFS.
        if (!fs.existsSync(path.join(card, '.crosspoint'))) {
          FS.unmount('/sd');
          FS.mount(nodefs, {root: stage}, '/sd');
          FS.mkdir('/sd/.crosspoint');
          FS.unmount('/sd'); FS.mount(nodefs, {root: card}, '/sd');
          // Emscripten needs a mountpoint in the parent filesystem.
          fs.mkdirSync(path.join(card, '.crosspoint'));
        }
        FS.mount(nodefs, {root: stageCache}, '/sd/.crosspoint');
        const open = FS.open.bind(FS);
        FS.open = (filename, flags, ...rest) => {
          const resolved = typeof filename === 'string' ? path.posix.normalize(filename) : FS.getPath(filename);
          const writable = typeof flags === 'string' ? /[wa+]/.test(flags) : (flags & (1 | 2 | 64 | 512 | 1024));
          if (writable && resolved.startsWith('/sd/') && !resolved.startsWith('/sd/.crosspoint/'))
            throw Error('Engine attempted a write outside its cache: ' + resolved);
          return open(filename, flags, ...rest);
        };
      }
    });
    const call = (name, returns, types, args) => engine.ccall(name, returns, types, args);
    const catalog = JSON.parse(call('prepare_initialize', 'string', ['string'], [JSON.stringify(profile)]));
    if (catalog.error) throw Error(catalog.error);
    const requestedFont = profile.sdFontFamilyName || 'Ubuntu';
    const availableFont = catalog.fonts.find(font => font.name === requestedFont);
    const selectedFont = availableFont ? requestedFont : 'Ubuntu';
    const selectedFamily = catalog.fonts.find(font => font.name === selectedFont);
    const selectedSize = selectedFamily.sizes.reduce((a,b) => Math.abs(b - profile.fontPointSize) < Math.abs(a - profile.fontPointSize) ? b : a);
    emit({type: 'catalog', ...catalog, profile, card, requestedFont, selectedFont, selectedSize, fontMissing: !availableFont});
    if (mode === 'inspect') return;
    if (mode === 'run') requireSpace(card);
    const layouts = buildLayouts(catalog, configuration, selectedFont, profile.fontPointSize);
    const formats = configuration.formats || ['epub', 'xtc', 'xtch'];
    const candidates = scan(card, formats);
    let completed = 0, failed = 0, cancelled = false;
    const aborted = () => configuration.cancelFile && fs.existsSync(configuration.cancelFile);
    emit({type: 'checking', total: candidates.length});
    const epubStamps = [];
    for (const layout of layouts) {
      if (!call('prepare_font', 'number', ['string', 'number'], [layout.name === 'Ubuntu' ? '' : layout.name, layout.size]))
        throw Error('Could not load font: ' + layout.name);
      epubStamps.push(call('prepare_stamp', 'string', ['number', 'number'], [1, profile.xtcCoverRotation || 0]));
      await defer();
      if (aborted()) { cancelled = true; break; }
    }
    const xtcStamp = call('prepare_stamp', 'string', ['number', 'number'], [0, profile.xtcCoverRotation || 0]);
    const books = [];
    let alreadyPrepared = 0;
    for (const book of candidates) {
      if (cancelled || aborted()) { cancelled = true; break; }
      rejectLinks(card, book.slice(1));
      const identity = sourceIdentity(fs.statSync(path.join(card, ...book.slice(1).split('/'))));
      const epub = /\.epub$/i.test(book);
      const key = (epub ? 'epub_' : 'xtc_') + (call('prepare_hash', 'number', ['string'], [book]) >>> 0);
      const existing = rejectLinks(card, '.crosspoint/' + key);
      const matches = relative => {
        const filename = rejectLinks(existing, relative);
        try { return fs.statSync(filename).size === identity.length && fs.readFileSync(filename).equals(identity); }
        catch (error) { if (error.code === 'ENOENT') return false; throw error; }
      };
      const covers = catalog.covers.every(height => fs.existsSync(rejectLinks(existing,
        'thumb_' + (!epub && profile.xtcCoverRotation ? 'ccw3_' : '') + height + '.bmp')));
      const native = (catalog.nativeCovers || []).every(spec => nativeCoverReady(existing, spec, !epub && !!profile.xtcCoverRotation));
      if (covers && native && (!epub || fs.existsSync(rejectLinks(existing, 'book.bin'))) &&
          (epub ? epubStamps : [xtcStamp]).every(matches)) ++alreadyPrepared;
      else books.push(book);
      if ((books.length + alreadyPrepared) % 32 === 0) await defer();
    }
    emit({type: 'scanned', total: books.length, alreadyPrepared, found: candidates.length});
    if (mode === 'plan') return;
    for (const book of books) {
      if (aborted()) { cancelled = true; break; }
      diagnostics.length = 0;
      errors.length = 0;
      const absolute = path.join(card, ...book.slice(1).split('/'));
      rejectLinks(card, book.slice(1));
      const before = fs.statSync(absolute);
      const key = (/\.epub$/i.test(book) ? 'epub_' : 'xtc_') + (call('prepare_hash', 'number', ['string'], [book]) >>> 0);
      const existing = rejectLinks(card, '.crosspoint/' + key);
      const local = path.join(stageCache, key);
      fs.mkdirSync(local, {recursive: true});
      const previous = walk(existing);
      const identity = sourceIdentity(before);
      const identityFiles = previous.filter(name => name === '.desktop-source-v1.bin' || /ready-v1-.*\.bin$/.test(name));
      const sameSource = identityFiles.length > 0 && identityFiles.every(name => {
        const saved = fs.readFileSync(path.join(existing, ...name.split('/')));
        return saved.equals(identity);
      });
      for (const relative of previous) if (sameSource && cacheFile(relative)) {
        const target = path.join(local, ...relative.split('/')); fs.mkdirSync(path.dirname(target), {recursive: true});
        fs.copyFileSync(path.join(existing, ...relative.split('/')), target);
      }
      let okay = true;
      const passes = /\.epub$/i.test(book) ? layouts : [layouts[0]];
      for (const layout of passes) {
        emit({type: 'progress', completed, total: books.length, book, font: layout.name, size: layout.size, percent: 0});
        if (!call('prepare_font', 'number', ['string', 'number'], [layout.name === 'Ubuntu' ? '' : layout.name, layout.size]))
          throw Error('Could not load font: ' + layout.name);
        if (!call('prepare_open', 'number', ['string', 'number'], [book, profile.xtcCoverRotation || 0])) { okay = false; break; }
        let phase = 1, last = 0;
        while (phase === 1) {
          if (aborted()) { cancelled = true; call('prepare_cancel', null, [], []); break; }
          phase = call('prepare_step', 'number', [], []);
          if (Date.now() - last >= 250 || phase !== 1) {
            const status = JSON.parse(call('prepare_status', 'string', [], []));
            emit({type: 'progress', completed, total: books.length, book, font: layout.name, size: layout.size,
              percent: status.total ? Math.min(100, Math.floor(status.done * 100 / status.total)) : 0,
              chapter: status.chapter, chapters: status.chapters, phase: status.phase});
            last = Date.now();
            if (status.failed) okay = false;
          }
          await defer();
        }
        if (phase < 0) okay = false;
        if (configuration.verify && /\.epub$/i.test(book) && !cancelled) {
          const pages = call('prepare_verify', 'number', ['string'], [book]);
          emit({type: 'verified', book, pages, font: layout.name, size: layout.size});
          if (pages < 0) okay = false;
        }
        if (cancelled) break;
      }
      call('prepare_cancel', null, [], []);
      if (okay && !cancelled && catalog.nativeCovers?.length &&
          !call('prepare_native_covers', 'number', ['string', 'number'], [book, profile.xtcCoverRotation || 0])) okay = false;
      const after = fs.statSync(absolute);
      if (before.size !== after.size || before.mtimeMs !== after.mtimeMs) throw Error('A book changed during preparation: ' + book);
      if (!sameSource && identityFiles.length) {
        for (const relative of previous.filter(cacheFile)) fs.unlinkSync(rejectLinks(card, '.crosspoint/' + key + '/' + relative));
      }
      fs.writeFileSync(path.join(local, '.desktop-source-v1.bin'), identity);
      // Commit only known cache formats. Stamps follow their chapters and covers.
      const files = walk(local).filter(cacheFile).sort((a,b) => Number(a.includes('ready-v1-')) - Number(b.includes('ready-v1-')));
      for (const relative of files) atomicCopy(path.join(local, ...relative.split('/')), card, '.crosspoint/' + key + '/' + relative);
      if (cancelled) break;
      if (okay) ++completed; else { ++failed; emit({type: 'bookError', book, diagnostics: [...errors,...diagnostics]}); }
      fs.rmSync(local, {recursive: true, force: true});
    }
    emit({type: 'finished', completed, failed, total: books.length, alreadyPrepared, found: candidates.length, cancelled});
  } catch (error) {
    if (diagnostics.length) emit({type: 'diagnostic', lines: diagnostics});
    throw error;
  } finally {
    try {
      if (engine) engine.ccall('prepare_cancel', null, [], []);
    } finally {
      if (engine) await closeEngine(engine);
      fs.rmSync(stage, {recursive: true, force: true});
    }
  }
}
module.exports = {safeDirectory, rejectLinks, scan, cacheFile, sourceIdentity, nativeCoverReady, atomicCopy, profileFor, buildLayouts, main};
if (require.main === module) {
  const configuration = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
  main(configuration, process.argv[2]).then(() => { process.exitCode = 0; }).catch(error => {
    emit({type: 'error', message: String(error.message || error)}); process.exitCode = 1;
  });
}

