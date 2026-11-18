'use strict';
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const assert = require('node:assert/strict');
const {cacheKind, period, scanCachePlan, deleteCachePlan} = require('../shell/cache-cleaner.cjs');
const {spaceInfo, requireSpace} = require('../shell/card-storage.cjs');
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'CrossNext-clean-test-'));
const card = path.join(root, 'Tarjeta 日本語');
const cache = path.join(card, '.crosspoint');
fs.mkdirSync(cache, {recursive: true});
const old = new Date(2026, 10, 12, 10), recent = new Date(2026, 10, 17, 12);
function put(relative, content = 'cache', modified = recent) {
  const filename = path.join(cache, ...relative.split('/'));
  fs.mkdirSync(path.dirname(filename), {recursive: true}); fs.writeFileSync(filename, content); fs.utimesSync(filename, modified, modified);
  return filename;
}
const preserved = ['settings.json','wifi.json','reading/stats.json','vocabulary/words/word.dat','chess.sav',
  'epub_123/progress.bin','epub_123/text.0.bin','epub_123/unknown.bin','epub_123/sections/unrelated.bin',
  'xtc_456/bookmark.bin','epub_123/._progress.bin'].map(name => [put(name,'personal'), 'personal']);
const oldPage = put('epub_123/sections/0.p00000001.bin','old',old);
const newPage = put('epub_123/sections/0.p00000002.bin');
const sidecar = put('epub_123/sections/._0.p00000002.bin');
const stamp = put('epub_123/sections/ready-v1-00000001.bin','stamp',old);
const metadata = put('epub_123/book.bin');
const image = put('epub_123/img_0_0.png.400x600.pxc');
const cover = put('xtc_456/thumb_264.bmp');
put('epub_123/events-v1/0.evc'); put('epub_123/html/0.html');
const sourceBook=path.join(card,'My book.epub');fs.writeFileSync(sourceBook,'original book');
const filename=path.join(root,'plan.jsonl');
const filter={mode:'period',from:'2026-11-17',to:'2026-11-17'};
const keep = () => {
  for(const [file,content] of preserved) assert.equal(fs.readFileSync(file,'utf8'),content,file);
  assert.equal(fs.readFileSync(sourceBook,'utf8'),'original book');
};
async function preview(value=filter) {
  if(fs.existsSync(filename)) fs.unlinkSync(filename);
  return scanCachePlan(card,value,filename);
}
(async()=>{
  for(const value of ['settings.json','vocabulary/words/x.dat','epub_123/../settings.json','epub_123\\sections/0.p00000001.bin',
    'epub_123/sections/deep/0.p00000001.bin','epub_4294967296/book.bin','epub_123/text.1.bin','xtc_456/progress.bin']) assert.equal(cacheKind(value),null,value);
  assert.equal(cacheKind('epub_123/sections/0.classic-p00000001.bin'),'prepared');
  assert.equal(cacheKind('epub_123/sections/._0.p00000001.bin'),'prepared');
  for (const name of ['epub_123/sections/0.p00000001.bin', 'epub_123/book.bin', 'xtc_456/thumb_264.bmp'])
    assert.equal(cacheKind(name + '.crossnext-preparing'),'interrupted');
  for (const name of ['settings.json', 'epub_123/progress.bin', 'epub_123/unknown.bin',
    'epub_123/book.bin.crossnext-preparing'])
    assert.equal(cacheKind(name + '.crossnext-preparing'),null);
  assert.throws(()=>period({mode:'period',from:'2026-02-30',to:'2026-03-01'}));
  assert.throws(()=>period({mode:'period',from:'2026-11-18',to:'2026-11-17'}));
  const range=period(filter);assert.ok(range.until>range.from);
  let plan=await preview();assert.equal(plan.files,3);assert.equal(plan.validationFiles,1);
  assert.ok(plan.allocatedBytes>=plan.logicalBytes);
  assert.ok(fs.existsSync(newPage)&&fs.existsSync(stamp));keep();
  const entries=fs.readFileSync(filename,'utf8').trim().split('\n').map(JSON.parse);
  assert.equal(entries[0].relative,'epub_123/sections/ready-v1-00000001.bin');
  let result=await deleteCachePlan(plan);assert.equal(result.deleted,3);
  assert.ok(!fs.existsSync(newPage)&&!fs.existsSync(sidecar)&&!fs.existsSync(stamp));
  for(const p of [oldPage,metadata,image,cover]) assert.ok(fs.existsSync(p));keep();
  // Files changed after preview are preserved rather than deleted using an old count.
  put('epub_123/sections/1.p00000003.bin');plan=await preview();
  put('epub_123/sections/1.p00000003.bin','changed cache with different size');
  result=await deleteCachePlan(plan);assert.equal(result.skipped,1);assert.equal(result.deleted,0);keep();
  // Validation stamps are removed before any page, including a stopped cleanup.
  put('epub_123/sections/ready-v1-00000002.bin');plan=await preview();
  const controller=new AbortController();controller.abort();result=await deleteCachePlan(plan,{signal:controller.signal});
  assert.equal(result.cancelled,true);assert.equal(result.deleted,0);keep();
  // A locked or newly changed validation stamp must not advertise missing pages.
  plan=await preview();
  const unlink=fs.unlinkSync;
  try {
    fs.unlinkSync=file=>{if(file.endsWith('ready-v1-00000002.bin'))throw Object.assign(Error('locked'),{code:'EBUSY'});return unlink(file);};
    result=await deleteCachePlan(plan);assert.equal(result.failed,1);assert.equal(result.deleted,0);assert.ok(result.skipped>0);
  }finally{fs.unlinkSync=unlink;}
  plan=await preview();put('epub_123/sections/ready-v1-00000002.bin','new validation');
  result=await deleteCachePlan(plan);assert.equal(result.deleted,0);assert.ok(result.skipped>0);keep();
  // A symlink/junction swapped in after preview cannot redirect deletions.
  const target=path.join(root,'outside');fs.mkdirSync(target);fs.writeFileSync(path.join(target,'2.p00000004.bin'),'outside');
  const linkBook=path.join(cache,'epub_789');fs.mkdirSync(linkBook);fs.mkdirSync(path.join(linkBook,'sections'));
  put('epub_789/sections/2.p00000004.bin');plan=await preview();
  fs.renameSync(path.join(linkBook,'sections'),path.join(linkBook,'saved-sections'));
  fs.symlinkSync(target,path.join(linkBook,'sections'),process.platform==='win32'?'junction':'dir');
  await assert.rejects(deleteCachePlan(plan),/Linked/);
  assert.equal(fs.readFileSync(path.join(target,'2.p00000004.bin'),'utf8'),'outside');keep();
  fs.unlinkSync(path.join(linkBook,'sections'));
  // Whole-cache mode includes generated covers and metadata, never personal files.
  plan=await preview({mode:'all'});assert.ok(plan.files>=5);
  result=await deleteCachePlan(plan);assert.equal(result.failed,0);
  for(const p of [oldPage,metadata,image,cover]) assert.ok(!fs.existsSync(p),p);keep();
  const normal=spaceInfo(card);assert.ok(normal.blockBytes>0&&normal.freeBytes>0&&normal.reserveBytes>=128*1024**2);
  const statfs=fs.statfsSync;
  try {
    fs.statfsSync=()=>({bsize:131072,blocks:500000,bavail:1});
    assert.throws(()=>requireSpace(card),error=>error.code==='spaceLow');
    fs.statfsSync=()=>({bsize:131072,blocks:500000,bavail:500000});requireSpace(card,100);
  }finally{fs.statfsSync=statfs;}
  // An interrupted copy can exhaust the reserve; all-cache cleanup makes retry possible.
  const leftover=put('epub_123/sections/0.p00000001.bin.crossnext-preparing');
  const copyBytes=64*1024**2;fs.truncateSync(leftover,copyBytes);
  const privateTemp=put('epub_123/progress.bin.crossnext-preparing','keep');
  try {
    const reserve=128*1024**2;
    fs.statfsSync=()=>({bsize:4096,blocks:1024*1024,bavail:(reserve+(fs.existsSync(leftover)?0:copyBytes))/4096});
    assert.throws(()=>requireSpace(card,copyBytes),error=>error.code==='spaceLow');
    plan=await preview();assert.equal(plan.files,0);assert.ok(fs.existsSync(leftover));
    plan=await preview({mode:'all'});assert.equal(plan.files,1);
    result=await deleteCachePlan(plan);assert.equal(result.deleted,1);
    requireSpace(card,copyBytes);assert.equal(fs.readFileSync(privateTemp,'utf8'),'keep');keep();
  }finally{fs.statfsSync=statfs;}
  console.log('PASS preview without writes, inclusive dates, old pages retained, validation-first ordering, sidecars, changed files, cancellation, link swaps, preserved personal data and low-space reserve.');
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>{
  assert.equal(path.dirname(root),path.resolve(os.tmpdir()));assert.ok(path.basename(root).startsWith('CrossNext-clean-test-'));
  fs.rmSync(root,{recursive:true,force:true});
});
