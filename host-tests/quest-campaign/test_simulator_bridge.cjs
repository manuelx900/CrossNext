'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const root=path.resolve(__dirname,'../..');

async function harness(supported=true){
 const frames=[],requested=[];let state=1,queueResult=2,boots=0,writes=0;
 const context2d={createImageData:()=>({data:new Uint8ClampedArray(800*480*4)})};
 const element=()=>({dataset:{},setAttribute(){},addEventListener(){},getContext:()=>context2d,getBoundingClientRect:()=>({width:480,height:800})});
 const module={_crossplay_quest_render_count:supported,_crossplay_quest_reading:supported,_crossplay_quest_reading_status:supported,FS:{writeFile(){++writes;}},cwrap(name){
  if(name==='crossplay_quest_reading')return minutes=>{requested.push(minutes);if(queueResult===2)state=2;return queueResult;};
  if(name==='crossplay_quest_reading_status')return ()=>state;
  if(name==='crossplay_quest_render_count')return ()=>7;
  if(name==='crossplay_frame_rotation')return ()=>90;
  return ()=>0;
 }};
 const window={devicePixelRatio:1,addEventListener(){}};
 const context={window,self:{crossOriginIsolated:true},console,Uint8ClampedArray,ResizeObserver:class{observe(){}},
  document:{createElement:element,head:{appendChild:script=>setImmediate(()=>script.onload())}},
  requestAnimationFrame:fn=>frames.push(fn),createCrossplay:()=>{++boots;return Promise.resolve(module);}};
 vm.runInNewContext(fs.readFileSync(path.join(root,'site/assets/emulator.js'),'utf8'),context);
 const mount=element();mount.appendChild=()=>{};
 const device=await window.Crossplay.boot(mount);
 return {device,requested,flush(){frames.splice(0).forEach(fn=>fn());},state(value){state=value;},rejectWith(value){queueResult=value;},boots:()=>boots,writes:()=>writes};
}
async function run(){
 const h=await harness();
 for(const invalid of [undefined,null,'20',NaN,Infinity,-1,1441,1.5])
  await assert.rejects(h.device.simulateReading(invalid),e=>e.code==='invalidMinutes');
 assert.equal(h.requested.length,0);
 let done=false;
 const first=h.device.simulateReading(20).then(value=>{done=true;return value;});
 await assert.rejects(h.device.simulateReading(5),e=>e.code==='busy');
 h.flush();await Promise.resolve();assert.equal(done,false);
 h.state(3);h.flush();await Promise.resolve();assert.equal(done,false);
 h.state(4);h.flush();assert.equal((await first).minutes,20);
 const zero=h.device.simulateReading(0);h.state(4);h.flush();assert.equal((await zero).minutes,0);
 assert.deepEqual(h.requested,[20,0]);
 for(const [state,code] of [[0,'unavailable'],[1,'notReady'],[5,'needsHero'],[6,'finishScreen'],[7,'storage'],[10,'notReady']]){
  const result=h.device.simulateReading(1);h.state(state);h.flush();await assert.rejects(result,e=>e.code===code);
 }
 h.rejectWith(9);await assert.rejects(h.device.simulateReading(2),e=>e.code==='busy');
 assert.equal(h.boots(),1);assert.equal(h.writes(),0);
 assert.equal(h.device.questRenderCount(),7);
 const old=await harness(false);assert.equal(old.device.simulateReading,null);assert.equal(old.device.questRenderCount,null);
 console.log('Simulator reading bridge passed: validation, one pending command, scan completion, zero minutes, errors, no reboots/card injection and old-build fallback.');
}
run().catch(error=>{console.error(error);process.exitCode=1;});
