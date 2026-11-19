'use strict';
const $ = id => document.getElementById(id);
const copy = {
  es: {
    skip:'Ir al contenido',companion:'Compañero de lectura',workspace:'BIBLIOTECA',prepareLibrary:'Preparar biblioteca',connect:'Conecta tu microSD',connectHint:'Elige la tarjeta del lector',personalize:'Selecciona qué preparar',personalizeHint:'Formatos, fuentes y tamaños',read:'Prepara y sigue leyendo',readHint:'Lleva la biblioteca al lector',localTitle:'Todo queda contigo',localHint:'La preparación se hace en tu computador. Tus libros no se suben a internet.',desktopCompanion:'DEL COMPUTADOR AL LECTOR',language:'Idioma',title:'Prepara los libros y las portadas en tu computador para llevarlos a tu firmware CrossNext.',intro:'Prepara los libros y las portadas en tu computador para llevarlos a tu CrossNext.',cardHeading:'Conecta tu biblioteca',notConnected:'Sin conectar',chooseCard:'Selecciona tu microSD',cardHint:'Elige la raíz de la tarjeta, no una carpeta de libros.',browse:'Elegir tarjeta',autoSettings:'Detectaremos los ajustes de lectura automáticamente.',refresh:'Actualizar',optionsHeading:'Selecciona qué preparar',readerDefaults:'Ajustes del lector por defecto',formats:'Formatos de libros',reflow:'Texto adaptable',fixedPages:'Páginas fijas',readingFonts:'Fuentes de lectura',selectAllFonts:'Seleccionar todas',deselectAllFonts:'Desmarcar todas',selectAllFontsLabel:'Seleccionar todas las fuentes de lectura',deselectAllFontsLabel:'Desmarcar todas las fuentes de lectura',searchFonts:'Buscar fuente…',fontsEmpty:'Las fuentes de tu tarjeta aparecerán aquí.',fontSize:'Tamaño de texto',readerSize:'Tamaño del lector',sizeHint:'Se utilizan solo tamaños compatibles con cada fuente.',engine:'Motor EPUB',modern:'Nuevo',classic:'Clásico',engineHint:'Usa el mismo motor en el lector para aprovechar las páginas preparadas.',readyInside:'LISTO POR DENTRO',lessWaiting:'Menos espera.\nMás lectura.',preparedInfo:'Adelanta la preparación antes de volver al lector.',index:'Índice y metadatos',indexHint:'Tu biblioteca organizada',pages:'Páginas de lectura',pagesHint:'Con las opciones que elijas',covers:'Portadas de libros',coversHint:'Para las apariencias del lector',preparationDetails:'Detalles de preparación',initialStatus:'Selecciona una tarjeta',initialDetail:'Conecta la microSD para preparar tu próxima lectura.',preserved:'Tus libros y tu progreso se conservan.',diagnostics:'Abrir diagnóstico',cancel:'Cancelar',start:'Preparar biblioteca',connected:'Conectada',reading:'Leyendo la tarjeta…',readingDetail:'Detectando fuentes y ajustes del lector.',ready:'Lista para preparar',readyDetail:'Revisa las opciones y prepara tu biblioteca.',working:'Preparando biblioteca…',checking:'Comprobando la biblioteca…',checkingDetail:'Reutilizaremos los archivos que ya estén preparados.',stopping:'Cancelando de forma segura…',stoppingDetail:'Esperando a que termine el paso actual. Conservaremos lo ya preparado.',complete:'Tu biblioteca está lista',completeDetail:'Expulsa la microSD desde el sistema antes de retirarla.',cancelled:'Preparación cancelada',cancelledDetail:'Lo preparado se conserva. Puedes continuar cuando quieras.',partial:'Preparación terminada con errores',partialDetail:'Abre el diagnóstico, comprueba los libros indicados y vuelve a intentarlo.',error:'No se pudo completar la preparación',errorDetail:'Comprueba la tarjeta, vuelve a elegirla e inténtalo de nuevo.',engineStopped:'El motor no pudo iniciarse. Abre el diagnóstico y vuelve a intentarlo.',detectedCards:n=>`${n} tarjetas detectadas. Elige la que quieres preparar.`,fontMissing:'La fuente guardada no está instalada. Se ha seleccionado Ubuntu.',selectedFonts:n=>`${n} ${n===1?'seleccionada':'seleccionadas'}`,supported:points=>`Tamaños disponibles: ${points} pt`,allSizes:'Todos los tamaños disponibles',allUsual:'Todos los tamaños habituales',moreSizes:'Más tamaños',fewerSizes:'Tamaños habituales',usualSizes:points=>`Habituales: ${points} pt`,noCommonSizes:'Cada fuente usa sus propios tamaños compatibles.',noFonts:'Selecciona al menos una fuente de lectura.',noFormats:'Selecciona al menos un formato de libro.',noMatches:'No se encontraron fuentes. Prueba otro nombre.',readerSettings:(font,size)=>`Ajustes del lector · ${font} · ${size} pt`,found:n=>`${n} archivos encontrados`,summary:e=>`${(e.alreadyPrepared||0)+e.completed} de ${e.found??e.total} preparados · ${e.failed} con errores · ${e.alreadyPrepared||0} ya preparados`,chapter:(a,b)=>`Capítulo ${a} de ${b}`,pending:n=>`${n} libros pendientes`,busyButton:'Preparando…',bookTitle:'Una página\na la vez.'
  },
  en: {
    skip:'Skip to content',companion:'Reading companion',workspace:'LIBRARY',prepareLibrary:'Prepare library',connect:'Connect your microSD',connectHint:'Choose your reader’s card',personalize:'Choose what to prepare',personalizeHint:'Formats, fonts and sizes',read:'Prepare and keep reading',readHint:'Bring your library to the reader',localTitle:'Your books stay with you',localHint:'Preparation happens on your computer. Your books are never uploaded.',desktopCompanion:'FROM DESKTOP TO READER',language:'Language',title:'Prepare books and covers on your computer to use them with your CrossNext firmware.',intro:'Prepare books and covers on your computer, then take them with you on CrossNext.',cardHeading:'Connect your library',notConnected:'Not connected',chooseCard:'Select your microSD',cardHint:'Choose the card root, rather than a books folder.',browse:'Choose card',autoSettings:'We’ll detect your reading settings automatically.',refresh:'Refresh',optionsHeading:'Choose what to prepare',readerDefaults:'Your reader settings by default',formats:'Book formats',reflow:'Reflowable text',fixedPages:'Fixed pages',readingFonts:'Reading fonts',selectAllFonts:'Select all',deselectAllFonts:'Deselect all',selectAllFontsLabel:'Select all reading fonts',deselectAllFontsLabel:'Deselect all reading fonts',searchFonts:'Search fonts…',fontsEmpty:'The fonts on your card will appear here.',fontSize:'Text size',readerSize:'Reader size',sizeHint:'Only sizes supported by each font are prepared.',engine:'EPUB engine',modern:'Modern',classic:'Classic',engineHint:'Use the same engine on your reader to use the prepared pages.',readyInside:'READY BEHIND THE SCENES',lessWaiting:'Less waiting.\nMore reading.',preparedInfo:'Get a head start before returning to your reader.',index:'Index and metadata',indexHint:'Your library, organized',pages:'Reading pages',pagesHint:'With the options you choose',covers:'Book covers',coversHint:'For your reader’s appearances',preparationDetails:'Preparation details',initialStatus:'Choose a card',initialDetail:'Connect your microSD to prepare your next read.',preserved:'Your books and reading progress are preserved.',diagnostics:'Open diagnostics',cancel:'Cancel',start:'Prepare library',connected:'Connected',reading:'Reading your card…',readingDetail:'Detecting reading fonts and settings.',ready:'Ready to prepare',readyDetail:'Review the options and prepare your library.',working:'Preparing library…',checking:'Checking your library…',checkingDetail:'Already prepared files will be reused.',stopping:'Cancelling safely…',stoppingDetail:'Waiting for the current step to finish. Completed files will be preserved.',complete:'Your library is ready',completeDetail:'Safely eject the microSD in your system before removing it.',cancelled:'Preparation cancelled',cancelledDetail:'Completed files are preserved. Continue whenever you’re ready.',partial:'Preparation finished with errors',partialDetail:'Open diagnostics, check the reported books and try again.',error:'Unable to complete preparation',errorDetail:'Check the card, choose it again and retry.',engineStopped:'The engine could not start. Open diagnostics and try again.',detectedCards:n=>`${n} cards detected. Choose the one to prepare.`,fontMissing:'The saved font is not installed. Ubuntu has been selected.',selectedFonts:n=>`${n} selected`,supported:points=>`Available sizes: ${points} pt`,allSizes:'All available sizes',allUsual:'All usual sizes',moreSizes:'More sizes',fewerSizes:'Usual sizes',usualSizes:points=>`Usual sizes: ${points} pt`,noCommonSizes:'Each font uses its own supported sizes.',noFonts:'Select at least one reading font.',noFormats:'Select at least one book format.',noMatches:'No matching fonts. Try another name.',readerSettings:(font,size)=>`Reader settings · ${font} · ${size} pt`,found:n=>`${n} files found`,summary:e=>`${(e.alreadyPrepared||0)+e.completed} of ${e.found??e.total} prepared · ${e.failed} with errors · ${e.alreadyPrepared||0} already prepared`,chapter:(a,b)=>`Chapter ${a} of ${b}`,pending:n=>`${n} books pending`,busyButton:'Preparing…',bookTitle:'One page\nat a time.'
  }
};
Object.assign(copy.es, {spaceLow:'Queda muy poco espacio en la microSD. Libera caché o archivos y vuelve a preparar con una fuente y un tamaño.'});
Object.assign(copy.en, {spaceLow:'The microSD has very little free space. Clear caches or remove files, then prepare with one font and one size.'});
let lang;
try { lang=localStorage.getItem('preferredLanguage'); } catch {}
lang=['en','es'].includes(lang) ? lang : 'en';
Object.assign(copy.es, {orientation:'Orientación',readerOrientation:'Orientación del lector',portrait:'Vertical',landscape:'Horizontal',both:'Vertical y horizontal',orientationHint:'Prepara ambas para girar sin volver a indexar. Ambas usan más espacio.'});
Object.assign(copy.en, {orientation:'Orientation',readerOrientation:'Reader orientation',portrait:'Portrait',landscape:'Landscape',both:'Portrait and landscape',orientationHint:'Prepare both to rotate without reindexing. Both use more storage.'});
const model={phase:'empty',busy:false,mode:null,catalog:null,fonts:new Set(),progress:null,result:null,diagnostics:false,error:null,errorCode:null,devices:[],advancedSizes:false};
const t=(key,...args)=>typeof copy[lang][key]==='function' ? copy[lang][key](...args) : copy[lang][key];
let planPending=false, planTimer;
function selection() {
  const value=$('sizes').value;
  return {fonts:[...model.fonts],formats:[...document.querySelectorAll('.formats input:checked')].map(input=>input.value),
    sizes:value==='usual' ? window.readerSizePolicy.usualSizes(commonSelectedSizes()) : ['all','current'].includes(value) ? value : [Number(value)],engine:Number($('engine').value),orientation:$('orientation').value};
}
function runPlan() {
  if(!planPending || model.busy || !model.catalog) return;
  const options=selection();
  if(!options.fonts.length || !options.formats.length) {planPending=false;window.libraryStatus.inventory(null);render();return;}
  planPending=false;
  invoke(()=>window.preparer.plan(options), true);
}
function schedulePlan() {
  if(!model.catalog) return;
  planPending=true;window.libraryStatus.pending();clearTimeout(planTimer);
  planTimer=setTimeout(runPlan,500);render();
}
function translate() {
  document.documentElement.lang=lang; $('language').value=lang;
  document.querySelectorAll('[data-i18n]').forEach(node=>{node.textContent=t(node.dataset.i18n);});
  document.querySelectorAll('[data-i18n-label]').forEach(node=>{node.setAttribute('aria-label',t(node.dataset.i18nLabel));});
  document.querySelectorAll('[data-placeholder]').forEach(node=>{node.placeholder=t(node.dataset.placeholder);});
  document.querySelector('.book-title').textContent=t('bookTitle');
  refreshSizes();renderFonts();render();
  window.libraryStatus.translate();
}
async function invoke(action, isPlan = false) {
  $('validation').hidden = true;
  try {
    await action();
  } catch (error) {
    model.phase = 'error';
    model.error = error.message;
    model.errorCode = error.message.includes('spaceLow:') ? 'spaceLow' : null;
    model.failureSeen = true;
    planPending = false;
    if (isPlan) window.libraryStatus.inventory(null);
    window.libraryStatus.event({type:'error',message:error.message,code:model.errorCode});
    render();
  }
}
function commonSelectedSizes() {
  const selected=(model.catalog?.fonts || []).filter(font=>model.fonts.has(font.name));
  return selected.length ? selected[0].sizes.filter(size=>selected.every(font=>font.sizes.includes(size))) : [];
}
function refreshSizes() {
  const previous=$('sizes').value;
  const selected=(model.catalog?.fonts || []).filter(font=>model.fonts.has(font.name));
  const common=commonSelectedSizes();
  const usual=window.readerSizePolicy.usualSizes(common);
  const choices=model.advancedSizes ? common : [...new Set([...usual,...(common.includes(Number(previous)) ? [Number(previous)] : [])])].sort((a,b)=>a-b);
  $('sizes').replaceChildren();
  const add=(value,text)=>{const option=document.createElement('option');option.value=value;option.textContent=text;$('sizes').append(option);};
  add('current',t('readerSize')+(model.catalog ? ` · ${model.catalog.selectedSize} pt` : ''));
  for(const size of choices) add(String(size),`${size} pt`);
  if(usual.length>1) add('usual',t('allUsual'));
  if(model.advancedSizes || previous==='all') add('all',t('allSizes'));
  if([...$('sizes').options].some(option=>option.value===previous)) $('sizes').value=previous;
  $('size-hint').textContent=common.length ? t(model.advancedSizes ? 'supported' : 'usualSizes',window.readerSizePolicy.formatSizes(model.advancedSizes ? common : usual)) : t('noCommonSizes');
  $('more-sizes').hidden=!selected.length || selected.every(font=>font.sizes.every(size=>usual.includes(size)));
  $('more-sizes').textContent=t(model.advancedSizes ? 'fewerSizes' : 'moreSizes');
  $('more-sizes').setAttribute('aria-expanded',String(model.advancedSizes));
  $('font-count').textContent=model.catalog ? t('selectedFonts',model.fonts.size) : '—';
}
function selectAllFonts(checked) {
  if(!model.catalog || (model.busy && model.mode!=='plan')) return;
  model.fonts=new Set(checked ? model.catalog.fonts.map(font=>font.name) : []);
  refreshSizes();renderFonts();$('validation').hidden=true;
  schedulePlan();
}
function renderFonts() {
  const list=$('font-list'),query=$('font-search').value.toLocaleLowerCase(lang);list.replaceChildren();
  const families=(model.catalog?.fonts || []).filter(font=>font.name.toLocaleLowerCase(lang).includes(query));
  if(!families.length) {const p=document.createElement('p');p.className='empty-fonts';p.textContent=t(model.catalog ? 'noMatches' : 'fontsEmpty');list.append(p);return;}
  for(const font of families) {
    const label=document.createElement('label'),input=document.createElement('input'),name=document.createElement('span');
    input.type='checkbox';input.checked=model.fonts.has(font.name);input.value=font.name;name.textContent=font.name;
    input.addEventListener('change',()=>{input.checked ? model.fonts.add(font.name) : model.fonts.delete(font.name);refreshSizes();$('validation').hidden=true;schedulePlan();});
    label.append(input,name);list.append(label);
  }
}
function setStatus(title,detail) {if($('status').textContent!==title) $('status').textContent=title;$('status-detail').textContent=detail;$('status-detail').hidden=['empty','ready'].includes(model.phase) || !detail;}
const statusIcons={
  empty:'<path d="M8 3h10a2 2 0 0 1 2 2v15a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7zM9 3v5m4-5v5m4-5v5M8 14h8M8 17h5"/>',
  ready:'<path d="M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2"/><rect x="8" y="2" width="8" height="4" rx="1"/><path d="m9 14 2 2 4-4"/>',
  busy:'<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
  complete:'<circle cx="12" cy="12" r="9"/><path d="m8 12 3 3 5-6"/>',
  error:'<path d="m10.3 3.7-8 14A2.2 2.2 0 0 0 4.2 21h15.6a2.2 2.2 0 0 0 1.9-3.3l-8-14a2 2 0 0 0-3.4 0M12 9v4m0 4h.01"/>'
};
function updateStatusIcon() {
  const kind=planPending || (model.busy && model.mode==='plan') ? 'busy' : model.phase==='complete' ? 'complete' : ['error','partial'].includes(model.phase) ? 'error' : model.busy ? 'busy' : model.catalog ? 'ready' : 'empty';
  if($('status-symbol').dataset.icon===kind) return;
  const icon=document.createElementNS('http://www.w3.org/2000/svg','svg');
  icon.setAttribute('viewBox','0 0 24 24');icon.innerHTML=statusIcons[kind];
  $('status-symbol').replaceChildren(icon);$('status-symbol').dataset.icon=kind;
}
function render() {
  window.cacheSafety?.setBusy(model.busy || planPending);
  const ready=Boolean(model.catalog), planning=planPending || (model.busy && model.mode==='plan');
  document.body.classList.toggle('has-current-book',Boolean(model.progress && ['working','stopping'].includes(model.phase) && !planning));
  document.body.classList.toggle('busy',model.busy);document.body.classList.toggle('complete',!planning && model.phase==='complete');document.body.classList.toggle('error',['error','partial'].includes(model.phase));
  $('browse').disabled=model.busy;$('refresh').disabled=model.busy;$('options').disabled=(model.busy && model.mode!=='plan') || !ready;$('start').disabled=model.busy || planPending || !ready;
  $('start').querySelector('span').textContent=t(model.busy && model.mode==='run' ? 'busyButton' : 'start');
  $('cancel').hidden=!model.busy || model.mode!=='run';$('cancel').disabled=model.phase==='stopping';$('diagnostics').hidden=!model.diagnostics && !model.failureSeen;
  $('card-badge').textContent=t(model.catalog ? 'connected' : 'notConnected');$('card-badge').classList.toggle('connected',Boolean(model.catalog));$('card-picker').classList.toggle('connected',Boolean(model.catalog));
  $('progress').hidden=planning || !['working','checking','stopping','complete','partial','cancelled'].includes(model.phase);
  $('percentage').hidden=planning || !model.progress || ['checking','stopping'].includes(model.phase);$('book-progress').hidden=planning || !model.progress || !['working','stopping'].includes(model.phase);
  updateStatusIcon();
  for(const id of ['step-card','step-options','step-run']) $(id).removeAttribute('aria-current');
  $(model.busy && model.mode==='run' || model.result ? 'step-run' : model.catalog ? 'step-options' : 'step-card').setAttribute('aria-current','step');
  if(model.catalog) {
    $('card-name').textContent=model.catalog.card.replace(/[\\/]+$/,'').split(/[\\/]/).pop() || model.catalog.card;$('card-path').textContent=model.catalog.card;
    $('profile-hint').textContent=model.catalog.fontMissing ? t('fontMissing') : t('readerSettings',model.catalog.selectedFont,model.catalog.selectedSize);
  }
  if(!model.catalog){$('card-name').textContent=t('chooseCard');$('card-path').textContent=t('cardHint');$('profile-hint').textContent=model.devices.length>1 ? t('detectedCards',model.devices.length) : t('autoSettings');}
  if(planning) setStatus(t('checking'),t('checkingDetail'));
  else if(model.phase==='empty') setStatus(t('initialStatus'),t('initialDetail'));
  else if(model.phase==='reading') setStatus(t('reading'),t('readingDetail'));
  else if(model.phase==='ready') setStatus(t('ready'),t('readyDetail'));
  else if(model.phase==='checking') setStatus(t('checking'),model.found!=null ? t('found',model.found) : t('checkingDetail'));
  else if(model.phase==='working') setStatus(t('working'),t('pending',Math.max(0,(model.progress?.total || 0)-(model.progress?.completed || 0)-(model.progress?.failed || 0))));
  else if(model.phase==='stopping') setStatus(t('stopping'),t('stoppingDetail'));
  else if(['complete','cancelled','partial'].includes(model.phase)) setStatus(t(model.phase),t('summary',model.result)+' · '+t(model.phase+'Detail'));
  else if(model.phase==='error') setStatus(t('error'),t(model.errorCode==='spaceLow' ? 'spaceLow' : model.errorCode==='engineStopped' ? 'engineStopped' : 'errorDetail'));
  if(model.progress) {
    const event=model.progress,value=Math.min(100,Math.max(0,((event.alreadyPrepared || 0)+event.completed)/(event.found || event.total || 1)*100));
    $('progress').value=model.phase==='complete' ? 100 : value;$('percentage').textContent=Math.floor(model.phase==='complete' ? 100 : value)+'%';
    $('current-book').textContent=(event.book || '').split(/[\\/]/).pop().replace(/\.(epub|xtch?)$/i,'');
    $('current-book').title=event.book || '';
    $('current-book').setAttribute('aria-label',event.book || '');
    $('current-chapter').hidden=!event.chapters;
    $('current-chapter').textContent=event.chapters ? t('chapter',event.chapter,event.chapters) : '';
    $('book-detail').textContent=`${event.font} · ${event.size} pt · ${t(event.orientation % 2 ? 'landscape' : 'portrait')}`;
  }
}
window.preparer.onEvent(event=>{
  switch(event.type) {
    case 'devices':model.devices=event.cards;break;
    case 'cardRemoved':model.catalog=null;model.phase='empty';model.result=null;model.progress=null;model.fonts.clear();planPending=false;window.libraryStatus.reset();refreshSizes();renderFonts();break;
    case 'started':if(event.mode!=='plan') model.failureSeen=false;model.busy=true;model.mode=event.mode;model.phase=event.mode==='inspect' ? 'reading' : 'checking';model.result=null;model.progress=null;model.found=null;model.diagnostics=false;model.error=null;model.errorCode=null;if(event.mode==='inspect') {model.catalog=null;model.advancedSizes=false;model.fonts.clear();$('font-search').value='';renderFonts();}break;
    case 'catalog':if(model.mode==='inspect') {model.catalog=event;model.fonts=new Set([event.selectedFont]);$('engine').value=String(event.profile.epubEngine);refreshSizes();renderFonts();}break;
    case 'checking':model.phase='checking';model.found=event.total;break;
    case 'scanned':model.found=event.found;break;
    case 'progress':if(model.phase!=='stopping') model.phase='working';model.progress=event;break;
    case 'inventory':if(!planPending) window.libraryStatus.inventory(event);break;
    case 'bookError':model.failureSeen=true;break;
    case 'bookComplete':if(model.progress) model.progress={...model.progress,...event,alreadyPrepared:event.prepared-event.completed};break;
    case 'finished':model.result=event;model.phase=event.cancelled ? 'cancelled' : event.failed ? 'partial' : 'complete';$('progress').value=event.found ? (event.alreadyPrepared+event.completed)/event.found*100 : 100;break;
    case 'error':model.phase='error';model.error=event.message;model.errorCode=event.code;model.failureSeen=true;planPending=false;if(model.mode==='plan')window.libraryStatus.inventory(null);break;
    case 'stopping':model.phase='stopping';break;
    case 'idle':model.busy=false;model.diagnostics=event.diagnostics;if(event.okay && ['inspect','plan'].includes(model.mode)) model.phase='ready';if(planPending) {clearTimeout(planTimer);planTimer=setTimeout(runPlan,500);}break;
  }
  render();
  window.libraryStatus.event(event);
});
$('language').addEventListener('change',()=>{lang=$('language').value;try{localStorage.setItem('preferredLanguage',lang);}catch{}translate();});
$('browse').addEventListener('click',()=>invoke(()=>window.preparer.chooseCard()));$('refresh').addEventListener('click',()=>invoke(()=>window.preparer.inspect()));$('font-search').addEventListener('input',renderFonts);
$('select-all-fonts').addEventListener('click',()=>selectAllFonts(true));
$('deselect-all-fonts').addEventListener('click',()=>selectAllFonts(false));
$('more-sizes').addEventListener('click',()=>{model.advancedSizes=!model.advancedSizes;refreshSizes();});
$('diagnostics').addEventListener('click',()=>window.libraryStatus.showDiagnostics());$('cancel').addEventListener('click',()=>invoke(async()=>{await window.preparer.cancel();if(model.busy && !model.result && model.phase!=='error'){model.phase='stopping';render();}}));
$('orientation').addEventListener('change',schedulePlan);$('sizes').addEventListener('change',schedulePlan);$('engine').addEventListener('change',schedulePlan);
document.querySelectorAll('.formats input').forEach(input=>input.addEventListener('change',schedulePlan));
$('start').addEventListener('click',()=>{
  const formats=[...document.querySelectorAll('.formats input:checked')].map(input=>input.value);
  const key=!formats.length ? 'noFormats' : !model.fonts.size ? 'noFonts' : null;
  if(key) {$('validation').textContent=t(key);$('validation').hidden=false;(key==='noFonts' ? $('font-search') : document.querySelector('.formats input')).focus();return;}
  const value=$('sizes').value;
  invoke(async()=>{
    const confirmed=await window.cacheSafety.confirmPreparation(selection(),model.catalog);
    if(confirmed) await window.preparer.prepare(confirmed);
  });
});
window.addEventListener('use-reader-selection',()=>{
  if(!model.catalog) return;
  model.fonts=new Set([model.catalog.selectedFont]);refreshSizes();$('sizes').value='current';$('orientation').value='reader';renderFonts();
  schedulePlan();
});
window.addEventListener('cache-cleanup-finished',()=>{
  if(!model.catalog || model.busy) return;
  model.phase='ready';model.result=null;model.progress=null;model.error=null;model.errorCode=null;render();
  schedulePlan();
});
document.body.dataset.platform=window.preparer.platform;translate();
