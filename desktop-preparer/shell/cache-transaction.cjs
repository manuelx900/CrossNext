'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {rejectLinks, requireSpace} = require('./card-storage.cjs');
const TRANSACTION = '.crosspoint/.preparer-transaction-v1';

function hashFile(filename) {
  const digest = crypto.createHash('sha256'), bytes = Buffer.allocUnsafe(64 * 1024);
  const fd = fs.openSync(filename, 'r');
  try {
    for (;;) {
      const count = fs.readSync(fd, bytes, 0, bytes.length, null);
      if (!count) break;
      digest.update(bytes.subarray(0, count));
    }
  } finally { fs.closeSync(fd); }
  return digest.digest('hex');
}
function syncFile(filename) {
  const fd = fs.openSync(filename, 'r+');
  try { fs.fsyncSync(fd); } finally { fs.closeSync(fd); }
}
function writeJournal(folder, journal) {
  const temporary = path.join(folder, 'journal.tmp');
  fs.writeFileSync(temporary, JSON.stringify(journal));
  syncFile(temporary);
  fs.renameSync(temporary, path.join(folder, 'journal.json'));
}
function validRelative(relative, cacheFile) {
  return typeof relative === 'string' && relative.length <= 1024 &&
    relative.split('/').every(part => part && part !== '.' && part !== '..' && !/[\\:]/.test(part)) && cacheFile(relative);
}
function readJournal(card, cacheFile) {
  const folder = rejectLinks(card, TRANSACTION);
  if (!fs.existsSync(folder)) return null;
  const filename = rejectLinks(folder, 'journal.json');
  if (!fs.existsSync(filename)) {
    // Publishing cannot begin before the durable journal exists.
    fs.rmSync(folder, {recursive: true});
    return null;
  }
  if (fs.statSync(filename).size > 8 * 1024 * 1024) throw Error('Preparation recovery journal is too large.');
  const journal = JSON.parse(fs.readFileSync(filename, 'utf8'));
  if (journal.version !== 1 || !/^(epub|xtc)_\d+$/.test(journal.key) ||
      !Array.isArray(journal.files) || journal.files.length > 100000 ||
      journal.files.some(file => !validRelative(file.relative, cacheFile) || typeof file.hadOriginal !== 'boolean' || typeof file.replace !== 'boolean') ||
      new Set(journal.files.map(file => file.relative)).size !== journal.files.length || typeof journal.committed !== 'boolean')
    throw Error('Preparation recovery journal is invalid. Keep the card unchanged and open diagnostics.');
  return {folder, journal};
}
function recoverPublication(card, cacheFile) {
  const pending = readJournal(card, cacheFile);
  if (!pending) return false;
  const {folder, journal} = pending;
  if (!journal.committed) {
    for (const file of [...journal.files].reverse()) {
      const target = rejectLinks(card, '.crosspoint/' + journal.key + '/' + file.relative);
      const backup = rejectLinks(folder, 'old/' + file.relative);
      const staged = rejectLinks(folder, 'next/' + file.relative);
      if (fs.existsSync(backup)) {
        if (fs.existsSync(target)) fs.unlinkSync(target);
        fs.mkdirSync(path.dirname(target), {recursive: true});
        fs.renameSync(backup, target);
      } else if (!file.hadOriginal && file.replace && !fs.existsSync(staged) && fs.existsSync(target)) {
        fs.unlinkSync(target);
      }
    }
  }
  fs.rmSync(folder, {recursive: true});
  return !journal.committed;
}
function publishBook(card, key, local, replacements, obsolete, cacheFile) {
  if (!/^(epub|xtc)_\d+$/.test(key) || [...replacements, ...obsolete].some(name => !validRelative(name, cacheFile)))
    throw Error('Unsafe preparation output.');
  recoverPublication(card, cacheFile);
  const destination = rejectLinks(card, '.crosspoint/' + key);
  const folder = rejectLinks(card, TRANSACTION);
  const files = [...new Set([...replacements, ...obsolete])].map(relative => ({
    relative, replace: replacements.includes(relative), hadOriginal: fs.existsSync(rejectLinks(destination, relative))
  }));
  const bytes = replacements.reduce((total, relative) => total + fs.statSync(rejectLinks(local, relative)).size, 0);
  requireSpace(card, bytes);
  fs.mkdirSync(folder, {recursive: true});
  let publishing = false, committed = false;
  try {
    for (const relative of replacements) {
      const source = rejectLinks(local, relative), target = rejectLinks(folder, 'next/' + relative);
      fs.mkdirSync(path.dirname(target), {recursive: true});
      requireSpace(card, fs.statSync(source).size);
      fs.copyFileSync(source, target);
      const stat = fs.statSync(source);
      const modified = new Date(Math.floor(stat.mtimeMs / 2000) * 2000);
      fs.utimesSync(target, stat.atime, modified);
      syncFile(target);
      if (hashFile(source) !== hashFile(target)) throw Error('The microSD could not verify a prepared file: ' + relative);
    }
    const journal = {version: 1, key, committed: false, files};
    writeJournal(folder, journal);
    publishing = true;
    for (const file of files) {
      const target = rejectLinks(destination, file.relative);
      if (file.hadOriginal) {
        const backup = rejectLinks(folder, 'old/' + file.relative);
        fs.mkdirSync(path.dirname(backup), {recursive: true});
        fs.renameSync(target, backup);
      }
      if (file.replace) {
        fs.mkdirSync(path.dirname(target), {recursive: true});
        fs.renameSync(rejectLinks(folder, 'next/' + file.relative), target);
      }
    }
    journal.committed = true;
    writeJournal(folder, journal);
    committed = true;
    fs.rmSync(folder, {recursive: true});
  } catch (error) {
    if (committed) return;
    try {
      if (publishing) recoverPublication(card, cacheFile);
      else fs.rmSync(folder, {recursive: true, force: true});
    } catch (recoveryError) {
      const failure = Error('Reconnect this card to Book Preparer to restore the interrupted cache update. ' + recoveryError.message);
      failure.code = 'recoveryNeeded';
      throw failure;
    }
    throw error;
  }
}
module.exports = {hashFile, recoverPublication, publishBook};
