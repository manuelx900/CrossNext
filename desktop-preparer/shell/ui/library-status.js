'use strict';
(() => {
  const $ = id => document.getElementById(id);
  const copy = {
    es: {count:(a,b)=>`${a} de ${b} libros preparados`,checking:(a,b)=>`Comprobando cachés: ${a} de ${b} libros`, options:'Con los formatos, fuentes, tamaños y motor seleccionados.',pending:'Calculando cuántos libros están preparados…',title:'Libros que no se pudieron preparar',none:'No hay errores registrados en esta ejecución.',preserved:'Los libros que fallan conservan sus cachés anteriores. Tus libros originales y tu progreso no se sustituyen.',failed:n=>`${n} libros con errores`,technical:'Abrir carpeta de registros técnicos',close:'Cerrar',previous:'Anterior',next:'Siguiente',unknown:'No se pudieron verificar todas las páginas o portadas. Revisa el detalle antes de volver a intentarlo.',spaceLow:'La tarjeta no tiene espacio suficiente. Libera cachés generadas o elige menos fuentes y tamaños.',io:'No se pudo leer o escribir en la microSD. Comprueba la conexión y que la tarjeta permita escribir.',recoveryNeeded:'Vuelve a conectar esta tarjeta a Book Preparer para restaurar la actualización de caché interrumpida.',details:'Detalle técnico',global:'Error de preparación'},
    en: {count:(a,b)=>`${a} of ${b} books prepared`,checking:(a,b)=>`Checking caches: ${a} of ${b} books`,options:'For the selected formats, fonts, sizes and engine.',pending:'Counting prepared books…',title:'Books that could not be prepared',none:'No errors were recorded in this run.',preserved:'Failed books keep their previous caches. Your original books and reading progress are not replaced.',failed:n=>`${n} books with errors`,technical:'Open technical log folder',close:'Close',previous:'Previous',next:'Next',unknown:'Not all pages or covers could be verified. Review the details before trying again.',spaceLow:'The card does not have enough space. Clear generated caches or choose fewer fonts and sizes.',io:'The microSD could not be read or written. Check its connection and make sure it is writable.',recoveryNeeded:'Reconnect this card to Book Preparer to restore the interrupted cache update.',details:'Technical details',global:'Preparation error'}
  };
  let inventory = null, failures = [], page = 0, technicalAvailable = false, pending = false;
  copy.es.openFailed='No se pudo abrir o analizar el libro. Comprueba que el archivo esté completo y revisa el detalle del error.';
  copy.en.openFailed='The book could not be opened or parsed. Check that the file is complete and review the error details.';
  copy.es.verificationFailed='Las páginas generadas no superaron la verificación. Se conservaron las cachés anteriores.';
  copy.en.verificationFailed='The generated pages failed verification. Previous caches were preserved.';
  copy.es.coverFailed='No se pudo preparar o verificar una portada. Se conservaron las cachés anteriores.';
  copy.en.coverFailed='A cover could not be prepared or verified. Previous caches were preserved.';
  const text = key => copy[document.documentElement.lang === 'es' ? 'es' : 'en'][key];
  copy.es.retained=(ready,pending)=>`${ready} ya verificados: se conservan. Quedan ${pending} por preparar con esta selección.`;
  copy.en.retained=(ready,pending)=>`${ready} already verified: kept as they are. ${pending} remain to prepare with this selection.`;
  copy.es.refresh=n=>`${n} tienen una preparación antigua o incompatible y necesitan actualizar su caché.`;
  copy.en.refresh=n=>`${n} have an older or incompatible preparation and need a cache update.`;
  copy.es.otherSettings=n=>`${n} necesitan páginas para la fuente, tamaño, orientación o motor seleccionados.`;
  copy.en.otherSettings=n=>`${n} need pages for the selected font, size, orientation or engine.`;
  copy.es.counting='Se comprueban las cachés guardadas. Todavía no se generan páginas nuevas. Tus libros y tu progreso se conservan.';
  copy.en.counting='Checking saved caches. No new pages are being generated yet. Your books and reading progress are preserved.';
  function paint() {
    $('library-summary').hidden = !inventory && !pending;
    $('library-counter').hidden = !inventory && !pending;
    if (pending) { $('library-counter').textContent = text('pending'); $('library-counter-hint').textContent = text('counting'); }
    if (inventory) {
      $('library-counter').textContent = inventory.complete ? text('count')(inventory.prepared, inventory.found) : text('checking')(inventory.checked, inventory.found);
      const hints = [inventory.complete ? text('retained')(inventory.prepared, Math.max(0,inventory.found-inventory.prepared)) : text('counting')];
      if (inventory.complete && inventory.needsRefresh) hints.push(text('refresh')(inventory.needsRefresh));
      if (inventory.complete && inventory.differentSettings) hints.push(text('otherSettings')(inventory.differentSettings));
      $('library-counter-hint').textContent = hints.join(' ');
    }
    if (!$('diagnostics-dialog').open) return;
    for (const [id,key] of [['diagnostics-title','title'],['diagnostics-preserved','preserved'],['diagnostics-close','close'],['diagnostics-folder','technical'],['diagnostics-empty','none']]) $(id).textContent = text(key);
    $('diagnostics-previous').textContent='‹'; $('diagnostics-next').textContent='›';
    $('diagnostics-previous').setAttribute('aria-label',text('previous')); $('diagnostics-next').setAttribute('aria-label',text('next'));
    $('diagnostics-folder').disabled = !technicalAvailable;
    $('diagnostics-empty').hidden = failures.length > 0;
    $('diagnostics-count').textContent = text('failed')(failures.filter(item => item.book).length);
    const pages = Math.max(1, Math.ceil(failures.length / 20));
    page = Math.max(0, Math.min(page, pages - 1));
    $('diagnostics-page').textContent = `${page + 1}/${pages}`;
    $('diagnostics-previous').disabled = page === 0;
    $('diagnostics-next').disabled = page + 1 === pages;
    const list = $('diagnostics-list'); list.replaceChildren();
    for (const failure of failures.slice(page * 20, page * 20 + 20)) {
      const item = document.createElement('li'), title = document.createElement('strong'), reason = document.createElement('p');
      title.textContent = failure.book || text('global');
      reason.textContent = ['spaceLow','recoveryNeeded','openFailed','verificationFailed','coverFailed'].includes(failure.code) ? text(failure.code) : ['EACCES','EPERM','EIO','ENOSPC','EROFS','ENOENT','ENODEV'].includes(failure.code) ? text('io') : text('unknown');
      item.append(title, reason);
      const details = document.createElement('details'), label = document.createElement('summary'), content = document.createElement('pre');
      label.textContent = text('details'); content.textContent = (failure.diagnostics || [failure.message || '']).join('\n');
      details.append(label, content); item.append(details); list.append(item);
    }
  }
  window.libraryStatus = {
    reset() { inventory = null; failures = []; page = 0; technicalAvailable = false; pending = false; paint(); },
    pending() { inventory = null; pending = true; paint(); },
    inventory(value) { inventory = value; pending = false; paint(); },
    event(event) {
      if (event.type === 'started' && event.mode !== 'plan') { failures = []; page = 0; technicalAvailable = false; }
      if (event.type === 'bookError' || event.type === 'error') failures.push(event);
      if (event.type === 'idle') technicalAvailable = event.diagnostics;
      if (event.type === 'bookComplete') inventory = {...event, complete: true};
      if (event.type === 'finished') inventory = {prepared:event.alreadyPrepared + event.completed,found:event.found,complete:true};
      paint();
    },
    showDiagnostics() { $('diagnostics-dialog').showModal(); paint(); $('diagnostics-close').focus(); },
    translate: paint
  };
  $('diagnostics-close').addEventListener('click', () => $('diagnostics-dialog').close());
  $('diagnostics-folder').addEventListener('click', () => window.preparer.diagnostics());
  $('diagnostics-previous').addEventListener('click', () => { --page; paint(); });
  $('diagnostics-next').addEventListener('click', () => { ++page; paint(); });
  paint();
})();
