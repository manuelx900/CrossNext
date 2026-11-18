'use strict';
const {_electron: electron} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const root = path.join(__dirname, '..');
const output = path.join(root, 'build', 'mac-layout-qa');
fs.mkdirSync(output, {recursive: true});
const fixture = fs.mkdtempSync(path.join(output, 'case-'));
const resources = path.join(fixture, 'sin ti\u0301tulo', 'CrossNext Book Preparer.app', 'Contents', 'Resources');
const appPath = path.join(resources, 'app');
fs.mkdirSync(appPath, {recursive: true});
fs.cpSync(path.join(root, 'shell'), path.join(appPath, 'shell'), {recursive: true});
fs.cpSync(path.join(root, 'build', 'desktop-runtime'), path.join(resources, 'runtime'), {recursive: true});
fs.writeFileSync(path.join(appPath, 'package.json'), JSON.stringify({name: 'crossnext-mac-layout-test', version: JSON.parse(fs.readFileSync(path.join(root,'package.json'),'utf8')).version, main: 'bootstrap.cjs'}));
fs.writeFileSync(path.join(appPath, 'bootstrap.cjs'), "require('./shell/main.cjs');");
const card = path.join(fixture, 'Card');
assert.equal(spawnSync(process.platform === 'win32' ? 'python' : 'python3', [path.join(__dirname, 'make_desktop_fixture.py'), card]).status, 0);
async function startPreparation(page) {
  await page.locator('#start').click();
  await page.locator('#prepare-confirm').click();
}
let app;
(async () => {
  // Runs on the test host: this reproduces the Mac bundle layout and the false
  // isPackaged flag, without claiming to simulate macOS native process behavior.
  app = await electron.launch({args: [appPath, '--es', '--disable-card-discovery', '--user-data-dir='+path.join(fixture,'profile')]});
  const page = await app.firstWindow();
  assert.equal(await app.evaluate(({app}) => app.isPackaged), false);
  await app.evaluate(({dialog}, card) => { dialog.showOpenDialog = async () => ({canceled: false, filePaths: [card]}); }, card);
  await page.locator('#browse').click();
  await page.waitForFunction(() => !document.querySelector('#start').disabled, {}, {timeout: 60000});
  await startPreparation(page);
  await page.waitForFunction(() => document.querySelector('#status').textContent === 'Tu biblioteca está lista' && !document.querySelector('#start').disabled, {}, {timeout: 120000});
  assert.ok(fs.readdirSync(path.join(card, '.crosspoint')).some(name => name.startsWith('epub_')));
  await page.screenshot({path: path.join(output, 'finished.png'), scale: 'css'});
  console.log('PASS actual engine under a Mac-style .app Resources/runtime path with isPackaged=false, spaces and NFD accents');
  await app.close();
  fs.renameSync(path.join(resources,'runtime'),path.join(fixture,'runtime-held'));
  app=await electron.launch({args:[appPath,'--es','--disable-card-discovery','--user-data-dir='+path.join(fixture,'profile')]});
  const failedPage=await app.firstWindow();
  await app.evaluate(({dialog,shell},card)=>{
    dialog.showOpenDialog=async()=>({canceled:false,filePaths:[card]});
    shell.openPath=async folder=>{globalThis.openedDiagnostic=folder;return '';};
  },card);
  await failedPage.locator('#browse').click();
  await failedPage.waitForFunction(()=>!document.querySelector('#diagnostics').hidden && !document.querySelector('#browse').disabled,{},{timeout:30000});
  assert.equal(await failedPage.locator('#status-detail').textContent(),'El motor no pudo iniciarse. Abre el diagnóstico y vuelve a intentarlo.');
  await failedPage.locator('#diagnostics').click();
  const diagnostic=await app.evaluate(()=>globalThis.openedDiagnostic);
  const launcher=JSON.parse(fs.readFileSync(path.join(diagnostic,'launcher.json'),'utf8'));
  assert.equal(launcher.version,JSON.parse(fs.readFileSync(path.join(root,'package.json'),'utf8')).version);
  assert.equal(launcher.runtime,path.join(resources,'runtime'));
  assert.equal(launcher.files['job.cjs'],false);
  assert.ok(fs.existsSync(path.join(diagnostic,'job-exit.json')));
  assert.ok(!launcher.runtime.includes(path.join('app','build')));
  console.log('PASS incomplete Mac bundle: canonical runtime path, versioned launcher/exit diagnostics, Spanish error and usable recovery controls');
})().catch(error => {console.error(error);process.exitCode = 1;}).finally(async () => {
  if (app) {await app.evaluate(({app}) => app.exit(0)).catch(() => {});await app.close().catch(() => {});}
});
