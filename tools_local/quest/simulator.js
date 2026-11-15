'use strict';
const params = new URLSearchParams(location.search);
const previousGeneration=params.get('save-generation')==='previous';
const controls = Object.fromEntries(['language','stage','gender','background','layout','orientation','panel','category','item','request','requestState','level'].map(id=>[id,document.getElementById(id)]));
const deviceBox=document.getElementById('device'), status=document.getElementById('status');
const book=document.getElementById('book'), chapter=document.getElementById('chapter'), enemy=document.getElementById('enemy'), moment=document.getElementById('moment');
const bounded=(name,max,fallback=0)=>{const n=Number(params.get(name));return Number.isInteger(n)&&n>=0&&n<=max?n:fallback;};
function choose(control,value,fallback){control.value=value; if(control.selectedIndex<0)control.value=fallback;}
choose(controls.stage,params.get('stage'),'custom');
choose(controls.gender,params.get('gender'),'0');choose(controls.layout,params.get('layout'),'0');
choose(controls.orientation,params.get('orientation'),'0');choose(controls.panel,params.get('panel'),controls.stage.value==='custom'?'encounter':'hero');
choose(controls.category,params.get('category'),'0');choose(moment,params.get('moment'),'1');
choose(controls.request,params.get('request'),'0');choose(controls.requestState,params.get('requestState'),'1');
function updateRequestStates(){
 const selected=Number(controls.request.value);
 for(const option of controls.requestState.options){
  option.disabled=selected>=302&&option.value!=='0'&&!(selected===305&&option.value==='2');
 }
 if(controls.requestState.selectedOptions[0]?.disabled)controls.requestState.value='0';
}
controls.request.addEventListener('change',updateRequestStates);
if(controls.stage.value==='ranked-secret'){controls.request.value='305';controls.requestState.value='2';}
updateRequestStates();
if(matchMedia('(max-width:599px)').matches)document.querySelector('aside details').open=false;
deviceBox.dataset.orient=controls.orientation.value==='1'?'landscape':'portrait';
function bytes(value){return Uint8Array.from(atob(value),ch=>ch.charCodeAt(0));}
function restart(custom=false){
 const next=new URL(location.href);
 for(const [key,control] of Object.entries(controls))next.searchParams.set(key,control.value);
 if(custom){next.searchParams.set('stage','custom');next.searchParams.set('encounter',Number(enemy.value));next.searchParams.set('moment',moment.value);}
 location.href=next;
}
fetch('./catalog.json',{cache:'no-store'}).then(response=>{if(!response.ok)throw Error('No se pudieron cargar los escenarios.');return response.json();}).then(async catalog=>{
 if(catalog.version!==6||!Array.isArray(catalog.languages)||!catalog.languages.length||!Array.isArray(catalog.battles)||catalog.battles.length!==120||!Array.isArray(catalog.books)||!Array.isArray(catalog.enemies)||!Array.isArray(catalog.gear)||!catalog.journals||!Array.isArray(catalog.roles)||catalog.roles.length!==720||catalog.roles.some(role=>!['combat','explore','rescue','restore'].includes(role)))throw Error('Los escenarios están desactualizados. Vuelve a preparar el simulador.');
 const add=(select,value,text)=>{const option=document.createElement('option');option.value=value;option.textContent=text;select.appendChild(option);};
 for(const locale of catalog.languages){
  add(controls.language,locale.code,locale.name+(locale.fictional?' · ficticio':'')+' · '+locale.code);
  controls.language.lastElementChild.dir='auto';
 }
 choose(controls.language,params.get('language'),'es');
 const locale=catalog.languages.find(item=>item.code===controls.language.value);
 if(!locale||!Array.isArray(locale.files)||!locale.files.length)throw Error('El idioma seleccionado no tiene sus recursos.');
 controls.language.disabled=false;
 controls.language.addEventListener('change',()=>{
  const next=new URL(location.href);
  next.searchParams.set('language',controls.language.value);
  location.href=next;
 });
 status.textContent='Cargando idioma: '+locale.name+'…';
 const languageFiles={};
 const resources=[...(locale.pack?[locale.pack]:[]),...locale.files];
 await Promise.all(resources.map(async path=>{
  if(!/^(?:[A-Za-z][A-Za-z0-9-]*\.cplang|fonts\/Interface(?:_SC|_TC)?_(?:8|10|12)\.cpfont|keyboards\/[A-Za-z0-9_-]+\.(?:cpkbd|bin))$/.test(path))throw Error('Ruta de idioma no válida.');
  const response=await fetch('./languages/'+path,{cache:'no-store'});
  if(!response.ok)throw Error('No se pudo cargar el recurso del idioma: '+path);
  languageFiles['/fs_/languages/'+path]=new Uint8Array(await response.arrayBuffer());
 }));
 for(const hunt of catalog.bestiary||[])add(controls.request,hunt.id,`${hunt.rank==='Legendary'?'Legendario':'Élite'} · ${hunt.name.es}`);
 choose(controls.request,params.get('request'),'0');
 if(controls.stage.value==='bestiary'&&Number(controls.request.value)<306)controls.request.value='306';
 updateRequestStates();
 for(const n of catalog.levels||Array.from({length:100},(_,n)=>n+1))add(controls.level,n,`Nivel ${n}`);
 choose(controls.level,params.get('level'),'50');
 for(const scene of catalog.backgrounds)add(controls.background,scene.id,`${scene.es} · Nv ${scene.level}`);
 const ranked=['ranked-hunts','ranked-secret','bestiary'].includes(controls.stage.value);
 controls.background.value=ranked||controls.stage.value==='collections-hunts'||controls.stage.value==='finale-request'||controls.stage.value.startsWith('milestone-')?'0':String(bounded('background',49));
 if((ranked||controls.stage.value==='collections-hunts')&&!params.has('panel'))controls.panel.value='requests';
 for(const volume of catalog.books)add(book,volume.id,`${volume.id+1} · ${volume.title}`);
 const encounter=Math.floor(bounded('encounter',719)/6)*6+5;book.value=String(Math.floor(encounter/60));
 for(let n=0;n<10;n++)add(chapter,n,`Capítulo ${n+1}`);
 chapter.value=String(Math.floor(encounter/6)%10);
 function updateEnemies(){
  enemy.replaceChildren();
  for(const battle of catalog.battles.filter(e=>e.book===Number(book.value)))add(enemy,battle.anchor,`${battle.chapter%10+1} · ${battle.title}`);
  enemy.value=String(Number(book.value)*60+Number(chapter.value)*6+5);updateNote();
 }
 function updateNote(){const battle=catalog.battles.find(b=>b.anchor===Number(enemy.value));document.getElementById('encounter-note').textContent=battle?`${battle.episode}. Elige al enemigo en Camino para ver su escena.`:'';}
 updateEnemies();
 book.addEventListener('change',()=>{chapter.value='0';updateEnemies();});chapter.addEventListener('change',updateEnemies);
 enemy.addEventListener('change',()=>{chapter.value=String(Math.floor(Number(enemy.value)/6)%10);updateNote();});
 const categoryNames={body:'Atuendos',background:'Fondos',pet:'PET',decoration:'Adornos'};
 for(const [categoryName,label] of Object.entries(categoryNames)){
  const group=document.createElement('optgroup');group.label=label;controls.item.appendChild(group);
  for(const item of catalog.gear.filter(g=>g.category===categoryName))add(group,item.id,item.name);
 }
 choose(controls.item,params.get('item'),'-1');
 controls.background.disabled=ranked||controls.stage.value==='collections-hunts'||controls.stage.value==='custom'||(['new','crown-home','crown-reading','finale-request'].includes(controls.stage.value)||controls.stage.value.startsWith('milestone-'));
 controls.gender.disabled=controls.stage.value==='new';
 for(const name of ['stage','gender','background','layout','orientation'])controls[name].addEventListener('change',()=>restart(false));
 for(const id of ['load-encounter','load-request','load-shop','apply-view','restart','new-game'])document.getElementById(id).disabled=false;
 document.getElementById('load-request').addEventListener('click',()=>{
  const exclusive=Number(controls.request.value)>=302;
  controls.stage.value=Number(controls.request.value)>=306?'bestiary':exclusive?(controls.request.value==='305'&&controls.requestState.value==='2'?'ranked-secret':'ranked-hunts'):'demo-request';
  controls.panel.value='request';restart(false);
 });
 document.getElementById('load-shop').addEventListener('click',()=>{controls.stage.value='shop-level';controls.panel.value='shop';controls.item.value='-1';restart(false);});
 document.getElementById('load-encounter').addEventListener('click',()=>{controls.panel.value=moment.value==='0'?'lore':'encounter';restart(true);});
 document.getElementById('apply-view').addEventListener('click',()=>restart(controls.stage.value==='custom'));
 document.getElementById('restart').addEventListener('click',()=>location.reload());
 document.getElementById('new-game').addEventListener('click',()=>{controls.stage.value='new';restart(false);});
 const files={...languageFiles,
 '/fs_/.crosspoint/settings.json':JSON.stringify({uiTheme:1,homeLayoutVersion:2,homeLayout:Number(controls.layout.value),language:locale.setting,languagePack:locale.pack||'',orientation:Number(controls.orientation.value),menuFontStyle:2,clockHasBeenSynced:1,clockUtcOffsetQ:-12,clockShowHeader:1,readingFormats:7,touchReaderControls:1,sleepTimeoutMinutes:31,fontSize:12,homeRecentBookCount:3}),
 '/fs_/.crosspoint/state.json':JSON.stringify({openEpubPath:'',lastSleepFromReader:false,showBootScreen:false})};
 if(controls.stage.value!=='new'){
  const key=controls.stage.value==='custom'?`custom-${encounter}-${controls.gender.value}-${moment.value}`
    :controls.stage.value==='demo-request'?`demo-request-${controls.request.value}-${controls.requestState.value}-${controls.gender.value}`
    :controls.stage.value==='shop-level'?`shop-level-${controls.level.value}-${controls.gender.value}`
    :controls.stage.value==='bestiary'?['bestiary',controls.request.value,controls.gender.value].join('-')
    :[controls.stage.value==='broken-history'?'unfinished':controls.stage.value,controls.gender.value,controls.background.value].join('-');
  if(!catalog.states[key])throw Error('El escenario seleccionado no está disponible.');
  const state=bytes(catalog.states[key]);
  const saveRoot=previousGeneration?'/fs_/.crosspoint/':'/fs_/.crosspoint/reading-adventure-v2/';
  files[saveRoot+(state[3]===65?'reading-quest-v10.dat':state[3]===57?'reading-quest-v9.dat':state[3]===56?'reading-quest-v8.dat':'reading-quest-v6.dat')]=state;
  if(previousGeneration&&state[3]===65)files['/fs_/Backups/AventuraLectora/partida-v10.rqu']=state;
  files['/fs_/.crosspoint/reading-history/sessions.bin']=bytes(catalog.journals[key]||catalog.journal);
  if(controls.stage.value==='broken-history')files['/fs_/.crosspoint/reading-history/sessions.bin'][0]^=1;
 }
 return Crossplay.boot(deviceBox,{env:{CROSSPLAY_AUTOSTART:'READING QUEST',CROSSNEXT_QUEST_ORIENTATION:controls.orientation.value,CROSSNEXT_QUEST_PANEL:controls.panel.value,CROSSNEXT_QUEST_CATEGORY:controls.category.value,CROSSNEXT_QUEST_ITEM:controls.item.value,CROSSNEXT_QUEST_REQUEST:controls.request.value},wipe:['/fs_/.crosspoint','/fs_/Backups/AventuraLectora-v2'],files,onFirstFrame(){status.textContent=previousGeneration?'Prueba de partida anterior: la app debe comenzar desde cero.':'Toca la pantalla para explorar. Elige un enemigo en Camino, pulsa Combatir y simula minutos de lectura.';}});
}).then(device=>{
 const readingForm=document.getElementById('reading-form'), minutesInput=document.getElementById('reading-minutes');
 const readingButton=document.getElementById('simulate-reading'), readingFeedback=document.getElementById('reading-feedback');
 let readingBusy=false;
 if(typeof device.questRenderCount==='function'){
  const diagnostics=document.createElement('p'), measure=document.createElement('button'), count=document.createElement('output');
  measure.type='button';measure.id='measure-renders';measure.textContent='Comprobar redibujados';
  count.id='render-count';count.style.marginLeft='0.5em';
  measure.addEventListener('click',()=>{count.textContent=String(device.questRenderCount());});
  diagnostics.append(measure,count);readingForm.after(diagnostics);
 }
 if(typeof device.simulateReading==='function')readingButton.disabled=false;
 else readingFeedback.textContent='Abre la versión actualizada del simulador para añadir lectura.';
 minutesInput.addEventListener('input',()=>{minutesInput.setCustomValidity('');if(!readingBusy)readingFeedback.textContent='';});
 readingForm.addEventListener('submit',async event=>{
  event.preventDefault();
  if(readingBusy||typeof device.simulateReading!=='function')return;
  const minutes=minutesInput.valueAsNumber;
  if(!Number.isInteger(minutes)||minutes<0||minutes>1440){
   minutesInput.setCustomValidity('Escribe un número entero entre 0 y 1440.');minutesInput.reportValidity();
   readingFeedback.textContent='Escribe un número entero entre 0 y 1440.';return;
  }
  readingBusy=true;readingButton.disabled=true;minutesInput.disabled=true;
  readingFeedback.textContent='Procesando el regreso a la app…';
  try{
   await device.simulateReading(minutes);
   readingFeedback.textContent=minutes===0?'Regreso simulado sin añadir minutos.':`Se añadieron ${minutes} min. Continúa en la pantalla del lector.`;
  }catch(error){
   const messages={
    unavailable:'Abre Aventura lectora antes de simular lectura.',
    notReady:'Espera a que la app termine de cargar e inténtalo de nuevo.',
    needsHero:'Crea y confirma tu héroe antes de simular lectura.',
    finishScreen:'Cierra el resumen o la ventana actual antes de simular otra lectura.',
    storage:'No se pudo actualizar la lectura. Revisa el estado de la app antes de volver a intentarlo.',
    invalidMinutes:'Escribe un número entero entre 0 y 1440.',
    busy:'Espera a que termine la lectura que se está procesando.'
   };
   readingFeedback.textContent=messages[error.code]||'No se pudo simular el regreso. Revisa el estado de la app.';
  }finally{readingBusy=false;readingButton.disabled=false;minutesInput.disabled=false;}
 });
 for(const key of ['up','down']){const button=document.getElementById(key);button.disabled=false;device.bindButton(button,device.scancodes[key]);}
 const back=document.getElementById('back');back.disabled=false;back.addEventListener('click',()=>device.swipeBack());
 device.canvas.focus({preventScroll:true});
}).catch(error=>{status.textContent=error.message;console.error(error);});
