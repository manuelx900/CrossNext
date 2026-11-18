'use strict';
// Desktop filesystem boundaries shared by the shell and preparation worker.
const fs = require('node:fs');
const path = require('node:path');

function rejectLinks(root, relative) {
  let current = root;
  for (const segment of relative.split(/[\\/]/)) {
    if (!segment || segment === '..' || segment.includes(':')) throw Error('Unsafe cache path.');
    current = path.join(current, segment);
    if (fs.existsSync(current) && fs.lstatSync(current).isSymbolicLink()) throw Error('Linked cache folders are not supported.');
  }
  return current;
}
function safeDirectory(root) {
  const absolute = path.resolve(root);
  if (!fs.statSync(absolute).isDirectory() || fs.lstatSync(absolute).isSymbolicLink()) throw Error('Select a real microSD folder.');
  if (process.platform !== 'win32' && (absolute === path.parse(absolute).root ||
      ['/System', '/Library', '/Applications', '/usr', '/bin', '/sbin', '/etc', '/private', '/dev'].some(folder => absolute === folder || absolute.startsWith(folder + '/'))))
    throw Error('Select the microSD volume under /Volumes, rather than a system folder.');
  const windows = process.env.SystemRoot && path.resolve(process.env.SystemRoot).toLowerCase();
  const system = windows && path.parse(windows).root;
  if ((system && absolute.toLowerCase() === system.toLowerCase()) ||
      (windows && (absolute.toLowerCase() === windows || absolute.toLowerCase().startsWith(windows + path.sep))) ||
      fs.existsSync(path.join(absolute, 'Windows', 'System32')))
    throw Error('The Windows system drive cannot be used as a microSD.');
  rejectLinks(absolute, '.crosspoint');
  return absolute;
}
function spaceInfo(root) {
  const stat = fs.statfsSync(root);
  const blockBytes = stat.bsize;
  const totalBytes = stat.blocks * blockBytes;
  return {blockBytes, totalBytes, freeBytes: stat.bavail * blockBytes,
    reserveBytes: Math.max(128 * 1024 ** 2, Math.min(512 * 1024 ** 2, Math.ceil(totalBytes * .01)))};
}
function requireSpace(root, bytes = 0) {
  const space = spaceInfo(root);
  const allocated = Math.ceil(bytes / space.blockBytes) * space.blockBytes;
  if (space.freeBytes < space.reserveBytes + allocated) {
    const error = Error('spaceLow: Not enough free space. Clear generated caches or choose fewer font and size combinations.');
    error.code = 'spaceLow';
    throw error;
  }
  return space;
}
module.exports = {safeDirectory, rejectLinks, spaceInfo, requireSpace};
