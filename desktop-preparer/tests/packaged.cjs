'use strict';
// Exercise the shipped payload, including external WASM/runtime lookup.
const {_electron:electron}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {spawnSync}=require('node:child_process');
const root=path.join(__dirname,'..'),output=path.join(root,'build','packaged-qa');
fs.mkdirSync(output,{recursive:true});
const card=fs.mkdtempSync(path.join(output,'Card-'));
if(process.env.CROSSNEXT_TEST_CARD_SOURCE) fs.cpSync(process.env.CROSSNEXT_TEST_CARD_SOURCE,card,{recursive:true,preserveTimestamps:true});
else assert.equal(spawnSync('python',[path.join(__dirname,'make_desktop_fixture.py'),card]).status,0);
const protectedFiles=[...fs.readdirSync(path.join(card,'books')).map(name=>path.join(card,'books',name)),
  path.join(card,'.crosspoint/settings.json'),path.join(card,'.crosspoint/reading/stats.json')].map(name=>[name,fs.readFileSync(name)]);
let app;
(async()=>{
  const payload=process.env.CROSSNEXT_PACKAGED_DIR||path.join(root,'dist/desktop/win-unpacked');
  app=await electron.launch({executablePath:path.join(payload,'CrossNext Book Preparer.exe'),args:['--es','--disable-card-discovery','--user-data-dir='+path.join(output,'profile-'+path.basename(card))]});
  const page=await app.firstWindow();
  await page.evaluate(()=>window.preparer.onEvent(event=>{if(event.type==='catalog')window.testCatalog=event;}));
  await page.locator('#language').selectOption('es');
  await app.evaluate(({dialog},card)=>{dialog.showOpenDialog=async()=>({canceled:false,filePaths:[card]});},card);
  await page.locator('#browse').click();
  await page.waitForFunction(()=>!document.querySelector('#start').disabled,{},{timeout:60000});
  await page.locator('#start').click();
  await page.waitForFunction(()=>document.querySelector('#status').textContent==='Tu biblioteca está lista' && !document.querySelector('#start').disabled,{},{timeout:120000});
  await page.screenshot({path:path.join(output,'finished.png')});
  assert.ok(fs.readdirSync(path.join(card,'.crosspoint')).some(name=>name.startsWith('epub_')));
  assert.equal(await app.evaluate(({app})=>app.isPackaged),true);
  const catalog=await page.evaluate(()=>window.testCatalog);
  const {nativeCoverReady}=require(path.resolve(payload,'resources/runtime/worker.cjs'));
  let native=0;
  for(const name of fs.readdirSync(path.join(card,'.crosspoint')).filter(name=>/^(epub|xtc)_/.test(name))) {
    const directory=path.join(card,'.crosspoint',name),rotated=name.startsWith('xtc_')&&!!catalog.profile.xtcCoverRotation;
    for(const spec of catalog.nativeCovers) {
      assert.equal(nativeCoverReady(directory,spec,rotated),true);
      if(fs.statSync(path.join(directory,'thumb_'+(rotated?'ccw3_':'')+spec.height+'.bmp')).size)native++;
    }
  }
  if(process.env.CROSSNEXT_TEST_CARD_SOURCE)assert.ok(native>0);
  for(const [name,bytes] of protectedFiles)assert.deepEqual(fs.readFileSync(name),bytes,name);
  console.log('PASS packaged Windows executable: real preparation, preserved files and '+native+' verified native rasters');
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(async()=>{if(app){await app.evaluate(({app})=>app.exit(0)).catch(()=>{});await app.close().catch(()=>{});}});
