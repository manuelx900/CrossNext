'use strict';
const {app, BrowserWindow, dialog, ipcMain, Menu, shell, utilityProcess, screen} = require('electron');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const readline = require('node:readline');
const {resolveRuntime, discoverCards} = require('./platform.cjs');
let window, currentCard, job, diagnosticFolder, quitting = false;
const runtime = resolveRuntime(process.resourcesPath, app.getAppPath());
const {spaceInfo, requireSpace} = require('./card-storage.cjs');
const {scanCachePlan, deleteCachePlan} = require('./cache-cleaner.cjs');
let cacheTask = null, cachePlan = null;
const cacheEvent = event => { if (window && !window.isDestroyed()) window.webContents.send('cache-event', event); };
function discardCachePlan() {
  if (!cachePlan) return;
  const {folder, filename} = cachePlan;
  if (path.dirname(folder) !== path.resolve(os.tmpdir()) || !path.basename(folder).startsWith('CrossNext-cache-') ||
      path.dirname(filename) !== folder || path.basename(filename) !== 'plan.jsonl') throw Error('Unsafe cache plan location.');
  cachePlan = null;
  try { fs.unlinkSync(filename); fs.rmdirSync(folder); } catch (error) {
    if (!['ENOENT', 'EPERM', 'EACCES', 'EBUSY', 'ENOTEMPTY'].includes(error.code)) throw error;
  }
}
async function cacheOperation(action) {
  if (job || cacheTask) throw Error('Wait for the current operation to finish.');
  if (!currentCard) throw Error('Choose the microSD card first.');
  cacheTask = new AbortController();
  try { return await action(cacheTask.signal); }
  finally { cacheTask = null; if (quitting) app.quit(); }
}
let scanningCards = false;
const emit = event => { if (window && !window.isDestroyed()) window.webContents.send('job-event', event); };
app.setName('CrossNext Book Preparer');
function launch(mode, configuration) {
  if (job || cacheTask) throw Error('An operation is already running.');
  if (!currentCard) throw Error('Choose the microSD card first.');
  const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'CrossNext-job-'));
  const cancelFile = path.join(folder, 'cancel');
  fs.writeFileSync(path.join(folder, 'job.json'), JSON.stringify({...configuration, card: currentCard, cancelFile}));
  fs.writeFileSync(path.join(folder, 'launcher.json'), JSON.stringify({version: app.getVersion(), platform: process.platform, arch: process.arch,
    runtime, resourcesPath: process.resourcesPath, appPath: app.getAppPath(),
    files: Object.fromEntries(['job.cjs', 'worker.cjs', 'engine.cjs', 'engine.wasm'].map(name => [name, fs.existsSync(path.join(runtime, name))]))}, null, 2));
  const child = utilityProcess.fork(path.join(runtime, 'job.cjs'), [mode, path.join(folder, 'job.json')], {stdio: 'pipe', serviceName: 'CrossNext library preparation'});
  job = {child, cancelFile, mode};
  let failure = false, finished = false, catalog = false, reportedError = false;
  emit({type: 'started', mode});
  const lines = readline.createInterface({input: child.stdout});
  const outputClosed = new Promise(resolve => lines.once('close', resolve));
  lines.on('line', line => {
    let event;
    try { event = JSON.parse(line); } catch { return; }
    if (event.type === 'catalog') catalog = true;
    if (event.type === 'error') reportedError = true;
    if (event.type === 'finished') finished = true;
    if (['error', 'bookError', 'diagnostic'].includes(event.type) || event.failed > 0) {
      failure = true;
      fs.appendFileSync(path.join(folder, 'report.jsonl'), line + '\n');
    }
    emit(event);
  });
  child.stderr.on('data', data => fs.appendFileSync(path.join(folder, 'stderr.log'), data));
  child.once('exit', async code => {
    await outputClosed;
    const okay = code === 0 && (mode === 'inspect' || mode === 'plan' ? catalog : finished);
    if (!okay) {
      failure = true;
      fs.writeFileSync(path.join(folder, 'job-exit.json'), JSON.stringify({code, mode, runtime, platform: process.platform, arch: process.arch, appIsPackaged: app.isPackaged}, null, 2));
      if (!reportedError) emit({type: 'error', code: 'engineStopped', message: 'The preparation engine stopped.'});
    }
    job = null;
    if (failure) diagnosticFolder = folder;
    else if (mode !== 'plan') diagnosticFolder = null;
    if (!failure) fs.rmSync(folder, {recursive: true, force: true});
    emit({type: 'idle', okay, diagnostics: Boolean(diagnosticFolder)});
    if (quitting) app.quit();
  });
}
function cancel() {
  if (job) fs.writeFileSync(job.cancelFile, 'cancel');
}
async function detectCards() {
  if (process.argv.includes('--disable-card-discovery') || scanningCards || job || cacheTask) return;
  scanningCards = true;
  try {
    const cards = await discoverCards();
    if (!window || window.isDestroyed() || quitting || job || cacheTask) return;
    emit({type: 'devices', cards});
    if (currentCard && !fs.existsSync(currentCard) && !job) {
      currentCard = null;
      emit({type: 'cardRemoved'});
    }
    if (!currentCard && !job && cards.length === 1) {
      currentCard = cards[0];
      launch('inspect', {});
    }
  } finally { scanningCards = false; }
}
function checkedOptions(value) {
  if (!value || !Array.isArray(value.formats) || !value.formats.length || value.formats.some(v => !['epub', 'xtc', 'xtch'].includes(v))) throw Error('Select at least one book format.');
  if (!Array.isArray(value.fonts) || !value.fonts.length || value.fonts.length > 128 || value.fonts.some(v => typeof v !== 'string' || v.length > 256)) throw Error('Select at least one reading font.');
  if (!['current', 'all'].includes(value.sizes) && !(Array.isArray(value.sizes) && value.sizes.length > 0 && value.sizes.length <= 64 && value.sizes.every(size => Number.isInteger(size) && size >= 4 && size <= 255))) throw Error('Select a supported font size.');
  if (![0, 1].includes(value.engine)) throw Error('Select an EPUB engine.');
    const orientation = value.orientation || 'reader';
    if (!['reader', 'portrait', 'landscape', 'both'].includes(orientation)) throw Error('Select a supported orientation.');
    return {formats: value.formats, fonts: value.fonts, sizes: value.sizes, orientation, profile: {epubEngine: value.engine}};
}
function setupIpc() {
  ipcMain.handle('choose-card', async () => {
    if (job || cacheTask) return;
    const choice = await dialog.showOpenDialog(window, {title: 'Select the microSD root / Selecciona la raíz de la microSD', defaultPath: process.platform === 'darwin' ? '/Volumes' : undefined, properties: ['openDirectory']});
    if (job || cacheTask) return;
    if (!choice.canceled && choice.filePaths.length) {
      discardCachePlan();
      currentCard = choice.filePaths[0];
      launch('inspect', {});
    }
  });
  ipcMain.handle('inspect', () => currentCard ? launch('inspect', {}) : detectCards());
  ipcMain.handle('plan', (_, value) => launch('plan', checkedOptions(value)));
  ipcMain.handle('prepare', (_, value) => {
    const configuration = checkedOptions(value);
    if (!currentCard) throw Error('Choose the microSD card first.');
    requireSpace(currentCard);
    discardCachePlan();
    launch('run', configuration);
  });
  ipcMain.handle('card-space', () => currentCard ? spaceInfo(currentCard) : null);
  ipcMain.handle('cache-preview', (_, filter) => cacheOperation(async signal => {
    discardCachePlan();
    const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'CrossNext-cache-'));
    const filename = path.join(folder, 'plan.jsonl');
    cachePlan = {folder, filename};
    try {
      const plan = await scanCachePlan(currentCard, filter, filename, {signal, onProgress: value => cacheEvent({type: 'scan', ...value})});
      cachePlan = {...plan, folder, id: require('node:crypto').randomUUID()};
      return {id: cachePlan.id, files: plan.files, logicalBytes: plan.logicalBytes, allocatedBytes: plan.allocatedBytes,
        validationFiles: plan.validationFiles, blockBytes: plan.blockBytes, freeBytes: plan.freeBytes, totalBytes: plan.totalBytes};
    } catch (error) { discardCachePlan(); throw error; }
  }));
  ipcMain.handle('cache-delete', (_, id) => cacheOperation(async signal => {
    if (!cachePlan || cachePlan.id !== id || cachePlan.root !== path.resolve(currentCard)) throw Error('Preview the cache again before deleting.');
    try { return await deleteCachePlan(cachePlan, {signal, onProgress: value => cacheEvent({type: 'delete', ...value})}); }
    finally { discardCachePlan(); }
  }));
  ipcMain.handle('cache-cancel', () => cacheTask?.abort());
  ipcMain.handle('cache-discard', () => { if (!cacheTask) discardCachePlan(); });
  ipcMain.handle('cancel', cancel);
  ipcMain.handle('diagnostics', () => diagnosticFolder ? shell.openPath(diagnosticFolder) : undefined);
}
function createWindow() {
  const work = screen.getPrimaryDisplay().workAreaSize;
  window = new BrowserWindow({
    width: Math.min(1180, work.width - 32), height: Math.min(850, work.height - 32),
    minWidth: 520, minHeight: 460, backgroundColor: '#f7f7f7', title: 'CrossNext Book Preparer',
    titleBarStyle: process.platform === 'darwin' ? 'hiddenInset' : 'hidden',
    titleBarOverlay: process.platform === 'win32' ? {color: '#202020', symbolColor: '#ffffff', height: 40} : true,
    trafficLightPosition: {x: 18, y: 14}, autoHideMenuBar: true,
    icon: path.join(__dirname, 'assets', 'icon.ico'),
    webPreferences: {preload: path.join(__dirname, 'preload.cjs'), contextIsolation: true, sandbox: true, nodeIntegration: false, spellcheck: false}
  });
  window.webContents.setWindowOpenHandler(() => ({action: 'deny'}));
  window.webContents.on('will-navigate', event => event.preventDefault());
  window.on('close', async event => {
    if (cacheTask) { event.preventDefault(); quitting = true; cacheTask.abort(); return; }
    discardCachePlan();
    if (!job) return;
    event.preventDefault();
    if (quitting) return;
    if (job.mode !== 'run') { quitting = true; cancel(); return; }
    const spanish = await window.webContents.executeJavaScript('document.documentElement.lang === "es"');
    const result = await dialog.showMessageBox(window, {
      type: 'question', title: 'CrossNext', defaultId: 0, cancelId: 0,
      buttons: spanish ? ['Seguir preparando', 'Cancelar y salir'] : ['Keep preparing', 'Cancel and quit'],
      message: spanish ? '¿Cancelar la preparación y salir?' : 'Cancel preparation and quit?',
      detail: spanish ? 'Los archivos ya preparados se conservan. Esperaremos a que termine el paso actual.' : 'Completed files are preserved. The app will finish the current step before closing.'
    });
    if (result.response === 1 && job) { quitting = true; cancel(); emit({type: 'stopping'}); }
    else if (result.response === 1) { quitting = true; app.quit(); }
  });
  window.loadFile(path.join(__dirname, 'ui', 'index.html'));
  if (!process.argv.includes('--disable-card-discovery')) {
    let rendererReady = false;
    window.webContents.once('did-finish-load', () => { rendererReady = true; detectCards().catch(() => {}); });
    window.on('focus', () => { if (rendererReady) detectCards().catch(() => {}); });
    const scanTimer = setInterval(() => { if (rendererReady) detectCards().catch(() => {}); }, 5000);
    scanTimer.unref();
    window.once('closed', () => clearInterval(scanTimer));
  }
  if (process.argv.includes('--es')) window.webContents.once('did-finish-load', () => window.webContents.executeJavaScript('document.querySelector("#language").value="es";document.querySelector("#language").dispatchEvent(new Event("change"))'));
}
if (process.platform === 'win32') app.setAppUserModelId('org.crossnext.bookpreparer');
app.whenReady().then(() => {
  Menu.setApplicationMenu(process.platform === 'darwin' ? Menu.buildFromTemplate([
    {role: 'appMenu'}, {role: 'editMenu'}, {label: 'View', submenu: [{role: 'resetZoom'}, {role: 'zoomIn'}, {role: 'zoomOut'}, {role: 'togglefullscreen'}]}, {role: 'windowMenu'}
  ]) : Menu.buildFromTemplate([{label: 'View', submenu: [{role: 'resetZoom'}, {role: 'zoomIn'}, {role: 'zoomOut'}, {role: 'togglefullscreen'}]}]));
  setupIpc(); createWindow();
  app.on('activate', () => { if (!BrowserWindow.getAllWindows().length) createWindow(); });
});
app.on('window-all-closed', () => { if (process.platform !== 'darwin' || quitting) app.quit(); });
module.exports = {checkedOptions};
