'use strict';
const {_electron: electron} = require('playwright');
const AxeBuilder = require('@axe-core/playwright').default;
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const root = path.join(__dirname, '..'), output = path.join(root, 'build/cache-qa');
fs.mkdirSync(output, {recursive: true});
const card = fs.mkdtempSync(path.join(output, 'Card-'));
assert.equal(spawnSync(process.env.CROSSNEXT_TEST_PYTHON || 'python', [path.join(__dirname, 'make_desktop_fixture.py'), card]).status, 0);
const old = new Date(2026, 10, 12, 10), recent = new Date(2026, 10, 17, 12);
const put = (relative, content, date = recent) => {
  const filename = path.join(card, '.crosspoint', ...relative.split('/'));
  fs.mkdirSync(path.dirname(filename), {recursive: true}); fs.writeFileSync(filename, content); fs.utimesSync(filename, date, date);
  return filename;
};
const oldPage = put('epub_123/sections/0.p00000001.bin', 'old pages', old);
const fresh = put('epub_123/sections/0.p00000002.bin', 'new pages');
const sidecar = put('epub_123/sections/._0.p00000002.bin', 'Mac cache metadata');
const stamp = put('epub_123/sections/ready-v1-00000001.bin', 'validated', old);
const metadata = put('epub_123/book.bin', 'shared index');
const save = put('epub_123/progress.bin', 'personal progress');
const protectedFiles = [save,path.join(card,'.crosspoint/settings.json'),path.join(card,'.crosspoint/reading/stats.json')]
  .map(filename => [filename,fs.readFileSync(filename)]);
let app;
const errors = [];
(async () => {
  const executablePath = process.env.CROSSNEXT_PACKAGED_EXECUTABLE;
  app = await electron.launch({...(executablePath ? {executablePath} : {}),args:[...(executablePath ? [] : [root]),'--es','--disable-card-discovery','--user-data-dir='+path.join(output,'profile-'+path.basename(card))]});
  const page = await app.firstWindow(); page.on('pageerror',error=>errors.push(error.message));
  await page.locator('#language').selectOption('es');
  await app.evaluate(({dialog},card)=>{dialog.showOpenDialog=async()=>({canceled:false,filePaths:[card]});},card);
  await page.locator('#browse').click();
  await page.waitForFunction(()=>!document.querySelector('#start').disabled,{}, {timeout:60000});
  await page.locator('#start').click();
  await page.locator('#prepare-warning').waitFor({state:'visible'});
  assert.equal(await page.locator('#prepare-layouts').textContent(),'1');
  assert.equal(await page.evaluate(()=>document.activeElement.id),'prepare-back');
  await page.keyboard.press('Escape');
  assert.ok(fs.existsSync(fresh));
  await page.evaluate(()=>{
    window.warningResult = window.cacheSafety.confirmPreparation(
      {formats:['epub'],fonts:['Ubuntu'],sizes:'all'},
      {selectedFont:'Ubuntu',selectedSize:12,fonts:[{name:'Ubuntu',sizes:[12,14,16,18]}]});
  });
  assert.ok(Number(await page.locator('#prepare-layouts').textContent())>1);
  await page.screenshot({path:path.join(output,'es-preparation-warning.png')});
  await page.locator('#prepare-reader').click();
  assert.equal(await page.locator('#sizes').inputValue(),'current');
  assert.equal(await page.locator('#font-list input:checked').count(),1);
  async function showCleanup() {
    await page.locator('#cache-open').click();
    await page.locator('#cache-from').fill('2026-11-17');
    await page.locator('#cache-to').fill('2026-11-17');
  }
  await showCleanup();
  await page.locator('#cache-preview').dblclick({delay:150});
  await page.locator('#cache-delete').waitFor({state:'visible'});
  assert.equal(await page.locator('#cache-delete').isDisabled(),true);
  for (const file of [fresh,sidecar,stamp]) assert.ok(fs.existsSync(file));
  await page.locator('#cache-close').click();
  await showCleanup();
  await page.screenshot({path:path.join(output,'es-cleanup-choose.png')});
  await page.locator('#cache-preview').click();
  await page.locator('#cache-delete').waitFor({state:'visible'});
  assert.equal(await page.locator('#cache-count').textContent(),'3');
  await page.screenshot({path:path.join(output,'es-cleanup-preview.png')});
  for (const file of [oldPage,fresh,sidecar,stamp,metadata]) assert.ok(fs.existsSync(file));
  await page.locator('#cache-close').click();
  for (const file of [fresh,sidecar,stamp]) assert.ok(fs.existsSync(file));
  await showCleanup(); await page.locator('#cache-preview').click();
  await page.locator('#cache-delete').waitFor({state:'visible'});
  await assert.rejects(page.evaluate(()=>window.preparer.deleteCache('wrong-plan-id')));
  assert.ok(fs.existsSync(fresh));
  await page.locator('#cache-understood').check();
  await page.locator('#cache-delete').click();
  await page.waitForFunction(()=>document.querySelector('#cache-status').textContent.includes('3 archivos eliminados'));
  for (const file of [fresh,sidecar,stamp]) assert.ok(!fs.existsSync(file));
  for (const file of [oldPage,metadata]) assert.ok(fs.existsSync(file));
  for (const [file,content] of protectedFiles) assert.deepEqual(fs.readFileSync(file),content);
  await page.screenshot({path:path.join(output,'es-cleanup-done.png')});
  await page.locator('#cache-close').click();
  put('epub_123/sections/3.p00000004.bin','review fixture');
  // Both modals must remain readable and operable at narrow widths and 200% zoom.
  for (const language of ['es','en']) {
    await page.locator('#language').selectOption(language);
    for (const [width,height,zoom] of [[1180,850,1],[540,540,1],[640,850,2]]) {
      await app.evaluate(({BrowserWindow},values)=>{
        const w=BrowserWindow.getAllWindows()[0];w.setMinimumSize(320,400);w.webContents.setZoomFactor(values[2]);w.setSize(values[0],values[1]);
      },[width,height,zoom]);
      await page.locator('#start').click();
      await page.locator('#prepare-warning').waitFor({state:'visible'});
      await page.evaluate(()=>new Promise(requestAnimationFrame));
      const modal=await page.locator('#prepare-warning').boundingBox();
      const viewport=await page.evaluate(()=>({width:innerWidth,height:innerHeight}));
      assert.ok(modal.x>=0&&modal.y>=0&&modal.x+modal.width<=viewport.width+1&&modal.y+modal.height<=viewport.height+1);
      const audit=await new AxeBuilder({page}).setLegacyMode().withTags(['wcag2a','wcag2aa','wcag21aa','wcag22aa']).analyze();
      assert.deepEqual(audit.violations.map(v=>({id:v.id,nodes:v.nodes.map(n=>n.target)})),[]);
      await page.screenshot({path:path.join(output,`${language}-warning-${width}-${zoom}.png`)});
      await page.keyboard.press('Escape');
      await page.locator('#cache-open').click();
      await page.locator('#cache-preview').click();
      await page.locator('#cache-delete').waitFor({state:'visible'});
      const cacheAudit=await new AxeBuilder({page}).setLegacyMode().withTags(['wcag2a','wcag2aa','wcag21aa','wcag22aa']).analyze();
      assert.deepEqual(cacheAudit.violations.map(v=>({id:v.id,nodes:v.nodes.map(n=>n.target)})),[]);
      await page.screenshot({path:path.join(output,`${language}-cleanup-${width}-${zoom}.png`)});
      await page.keyboard.press('Escape');
    }
  }
  await app.evaluate(({BrowserWindow})=>{const w=BrowserWindow.getAllWindows()[0];w.webContents.setZoomFactor(1);w.setSize(1180,850);});
  await page.locator('#cache-open').click();await page.locator('[name=cache-mode][value=all]').check();
  await page.locator('#cache-preview').click();await page.locator('#cache-delete').waitFor({state:'visible'});
  assert.equal(await page.locator('#cache-understood').isChecked(),false);
  await page.locator('#cache-understood').check();
  await page.locator('#cache-delete').click();
  await page.waitForFunction(()=>!document.querySelector('#cache-close').textContent.includes('Stop'));
  await page.locator('#cache-close').click();
  assert.ok(!fs.existsSync(oldPage)&&!fs.existsSync(metadata));
  for (const [file,content] of protectedFiles) assert.deepEqual(fs.readFileSync(file),content);
  await app.evaluate(()=>{
    const fs=process.getBuiltinModule('fs');globalThis.originalStatfs=fs.statfsSync;
    fs.statfsSync=()=>({bsize:131072,blocks:500000,bavail:1});
  });
  await page.locator('#start').click();
  await page.locator('#prepare-warning').waitFor({state:'visible'});
  assert.equal(await page.locator('#prepare-confirm').isDisabled(),true);
  assert.equal(await page.locator('#prepare-space-error').isVisible(),true);
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('#cache-open').isEnabled(),true);
  await app.evaluate(()=>{process.getBuiltinModule('fs').statfsSync=globalThis.originalStatfs;});
  assert.deepEqual(errors,[]);
  console.log('PASS real dialogs, default cancel, combination warning, recommended selection, no-delete preview, confirmation, invalid plan ID, date/all cleanup, personal state, low-space guard and ES/EN accessibility at narrow/200% zoom.');
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(async()=>{
  if(app) {await app.evaluate(({app})=>app.exit(0)).catch(()=>{});await app.close().catch(()=>{});}
});
