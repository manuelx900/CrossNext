'use strict';
const fs = require('node:fs');
const path = require('node:path');

function resolveRuntime(resourcesPath, appPath) {
  const bundled = path.join(resourcesPath, 'runtime');
  // Unsigned cross-built Mac bundles can still report app.isPackaged=false.
  if (fs.existsSync(bundled)) return bundled;
  if (['app', 'app.asar'].includes(path.basename(appPath))) {
    const adjacent = path.join(path.dirname(appPath), 'runtime');
    if (path.basename(path.dirname(appPath)).toLowerCase() === 'resources' || fs.existsSync(adjacent)) return adjacent;
  }
  return path.join(appPath, 'build', 'desktop-runtime');
}

async function discoverCards(platform = process.platform, volumesRoot = '/Volumes', driveRoots) {
  let roots;
  if (platform === 'darwin') {
    try {
      roots = (await fs.promises.readdir(volumesRoot, {withFileTypes: true}))
        .filter(entry => entry.isDirectory() && !entry.isSymbolicLink())
        .map(entry => path.join(volumesRoot, entry.name));
    } catch { return []; }
  } else if (platform === 'win32') {
    const system = (process.env.SystemDrive || 'C:').toLowerCase();
    roots = driveRoots || Array.from({length: 26}, (_, index) => String.fromCharCode(65 + index) + ':\\')
      .filter(root => root.slice(0, 2).toLowerCase() !== system);
  } else return [];
  const detected = await Promise.all(roots.map(async root => {
    try {
      const folder = await fs.promises.lstat(root);
      const cache = await fs.promises.lstat(path.join(root, '.crosspoint'));
      return folder.isDirectory() && !folder.isSymbolicLink() && cache.isDirectory() && !cache.isSymbolicLink() ? root : null;
    } catch { return null; }
  }));
  return detected.filter(Boolean).sort();
}

module.exports = {resolveRuntime, discoverCards};
