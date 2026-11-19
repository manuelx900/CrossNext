'use strict';
(() => {
  const $ = id => document.getElementById(id);
  const text = {
    es: {
      cleanup:'Eliminar caché de CrossPoint', recommend:'Para empezar, elige una fuente y un tamaño. Cada combinación EPUB guarda páginas aparte y ocupa más espacio.',
      beforePreparing:'ANTES DE PREPARAR', reviewSelection:'Revisa el espacio que vas a utilizar',
      combinationWarning:'Cada fuente, tamaño y orientación genera su propia paginación para cada EPUB. Elegir muchas combinaciones puede llenar la microSD.',
      fixedWarning:'Se prepararán las portadas y los índices de los libros seleccionados. Los archivos generados también ocupan espacio en la microSD.',
      layoutsLabel:'Combinaciones EPUB', availableLabel:'Espacio disponible', recommendedTitle:'Recomendado: una fuente y un tamaño',
      recommendedBody:'Usa los que lees habitualmente. Puedes preparar otras combinaciones más adelante, cuando las necesites.',
      unknownSize:'El espacio final depende de tus libros y de los bloques de la tarjeta; no se puede calcular exactamente de antemano.',
      reviewOptions:'Revisar selección', useReader:'Usar fuente y tamaño del lector', prepareNow:'Preparar biblioteca', cardMaintenance:'ESPACIO DE LA TARJETA',
      preserve:'Se conservan tus libros, ajustes, progreso, estadísticas, palabras y partidas.', cleanupMode:'Qué caché eliminar',
      byDate:'Páginas preparadas por fecha', byDateHint:'Quita las combinaciones creadas o actualizadas en el periodo elegido. Las páginas anteriores se conservan.',
      fromDate:'Desde', toDate:'Hasta, incluido', allCache:'Toda la caché de libros',
      allCacheHint:'Páginas, portadas, imágenes e índices generados. El lector los volverá a preparar cuando los necesite.',
      dateCaution:'Se usa la última modificación del archivo. Esto libera espacio; no recupera versiones anteriores sobrescritas.',
      recoverableLabel:'Espacio estimado a liberar', filesLabel:'Archivos de caché',
      blockHint:'El espacio en disco puede superar el tamaño de los archivos porque la tarjeta reserva bloques completos.',
      close:'Cerrar', changePeriod:'Cambiar selección', preview:'Revisar qué se eliminará', deleteCache:'Eliminar esta caché',
      consequence:'Entiendo que tendré que volver a preparar la caché eliminada y que esta acción no se puede deshacer.',
      free:n=>`${n} disponibles`, combinations:n=>`Preparar ${n} combinaciones`,
      lowSpace:'Queda muy poco espacio. Libera caché o archivos antes de preparar. Se mantendrá una pequeña reserva para el lector.',
      recovery:'No se pudo completar la operación. Comprueba que la tarjeta siga conectada, cierra otros programas que la utilicen y vuelve a revisar.',
      scanning:'Revisando la caché… Todavía no se ha eliminado nada.', scanProgress:n=>`${n} archivos revisados. Todavía no se ha eliminado nada.`,
      scanCancelled:'Revisión cancelada. No se ha eliminado nada.', empty:'No hay archivos de caché que coincidan con esta selección.',
      confirm:'Revisa el espacio y confirma la eliminación. No retires la tarjeta durante la limpieza.',
      deleting:'Eliminando la caché seleccionada…', deleteProgress:n=>`${n} archivos de caché eliminados…`,
      deleted:(n,b)=>`${n} archivos eliminados. Se han liberado aproximadamente ${b}.`,
      skipped:(n,f)=>`${n} archivos cambiados o desaparecidos se han conservado u omitido; ${f} no se pudieron eliminar. Vuelve a revisar si quieres intentarlo de nuevo.`,
      cancelled:(n,b)=>`Limpieza cancelada: ${n} archivos ya eliminados, aproximadamente ${b} liberados. El resto se conserva.`,
      validations:()=> 'El lector comprobará de nuevo la preparación de estos libros.',
      selectedPeriod:(from,to)=>`Páginas preparadas: del ${from} al ${to}, incluidos.`,
      cancelScan:'Cancelar revisión', cancelDelete:'Detener limpieza', invalidDates:'Elige un periodo válido; la fecha final debe ser igual o posterior a la inicial.', noCard:'Elige una tarjeta para revisar su espacio.'
    },
    en: {
      cleanup:'Clear CrossPoint cache', recommend:'Start with one font and one size. Each EPUB combination stores separate pages and uses more space.',
      beforePreparing:'BEFORE PREPARING', reviewSelection:'Review the space you are about to use',
      combinationWarning:'Each font, size and orientation creates a separate page layout for every EPUB. Selecting many combinations can fill the microSD.',
      fixedWarning:'Covers and indexes will be prepared for the selected books. Generated files also use space on the microSD.',
      layoutsLabel:'EPUB combinations', availableLabel:'Available space', recommendedTitle:'Recommended: one font and one size',
      recommendedBody:'Use the settings you normally read with. Prepare other combinations later, when you need them.',
      unknownSize:'The final space depends on your books and the card’s allocation blocks; it cannot be calculated exactly in advance.',
      reviewOptions:'Review selection', useReader:'Use reader font and size', prepareNow:'Prepare library', cardMaintenance:'CARD STORAGE',
      preserve:'Your books, settings, reading progress, statistics, saved words and game saves are preserved.', cleanupMode:'Cache to clear',
      byDate:'Prepared pages by date', byDateHint:'Remove combinations created or updated in the selected period. Earlier pages are preserved.',
      fromDate:'From', toDate:'Through, inclusive', allCache:'All book caches',
      allCacheHint:'Generated pages, covers, images and indexes. Your reader will prepare them again when needed.',
      dateCaution:'Dates refer to the file’s last modification. This frees space; it does not restore earlier overwritten versions.',
      recoverableLabel:'Estimated space to free', filesLabel:'Cache files',
      blockHint:'Space on disk can exceed file sizes because the card allocates whole blocks.',
      close:'Close', changePeriod:'Change selection', preview:'Review what will be removed', deleteCache:'Delete this cache',
      consequence:'I understand that I will need to prepare the removed cache again and that this action cannot be undone.',
      free:n=>`${n} available`, combinations:n=>`Prepare ${n} combinations`,
      lowSpace:'There is very little free space. Clear caches or remove files before preparing. A small reserve will be kept for the reader.',
      recovery:'Unable to complete the operation. Check that the card is still connected, close other programs using it and review again.',
      scanning:'Reviewing caches… Nothing has been deleted yet.', scanProgress:n=>`${n} files reviewed. Nothing has been deleted yet.`,
      scanCancelled:'Review cancelled. Nothing has been deleted.', empty:'No cache files match this selection.',
      confirm:'Review the space and confirm deletion. Keep the card connected during cleanup.',
      deleting:'Deleting the selected cache…', deleteProgress:n=>`${n} cache files deleted…`,
      deleted:(n,b)=>`${n} files deleted. Approximately ${b} freed.`,
      skipped:(n,f)=>`${n} changed or missing files were preserved or skipped; ${f} could not be deleted. Review again to retry.`,
      cancelled:(n,b)=>`Cleanup stopped: ${n} files already deleted, approximately ${b} freed. Remaining files are preserved.`,
      validations:()=> 'Your reader will check these books’ preparation again.',
      selectedPeriod:(from,to)=>`Prepared pages: ${from} through ${to}, inclusive.`,
      cancelScan:'Cancel review', cancelDelete:'Stop cleanup', invalidDates:'Choose a valid period; the end date must be on or after the start date.', noCard:'Choose a card to review its space.'
    }
  };
  const language = () => document.documentElement.lang === 'es' ? 'es' : 'en';
  const t = (key, ...args) => typeof text[language()][key] === 'function' ? text[language()][key](...args) : text[language()][key];
  const number = value => new Intl.NumberFormat(language()).format(value);
  const bytes = value => {
    const unit = value >= 1024 ** 3 ? 'GiB' : value >= 1024 ** 2 ? 'MiB' : value >= 1024 ? 'KiB' : 'B';
    const divisor = {GiB:1024 ** 3,MiB:1024 ** 2,KiB:1024,B:1}[unit];
    return new Intl.NumberFormat(language(), {maximumFractionDigits: 1}).format(value / divisor) + ' ' + unit;
  };
  let readerBusy = false, space = null, active = false, phase = 'choose', plan = null;
  let resolvePreparation = null, selection = null, preparationPending = false;
  const today = new Date();
  const dateValue = `${today.getFullYear()}-${String(today.getMonth() + 1).padStart(2, '0')}-${String(today.getDate()).padStart(2, '0')}`;
  $('cache-from').value = $('cache-to').value = dateValue;
  function translate() {
    document.querySelectorAll('[data-cache-i18n]').forEach(node => { node.textContent = t(node.dataset.cacheI18n); });
    updateSpaceLabel();
  }
  function updateSpaceLabel() {
    $('card-space').textContent = space ? t('free', bytes(space.freeBytes)) : '';
    $('cache-open').disabled = readerBusy || active || !space;
  }
  async function refreshSpace() {
    try { space = await window.preparer.cardSpace(); } catch { space = null; }
    updateSpaceLabel();
    return space;
  }
  function finishPreparation(value) {
    const done = resolvePreparation; resolvePreparation = null;
    preparationPending = false;
    $('prepare-warning').close();
    if (done) done(value);
  }
  async function confirmPreparation(configuration, catalog) {
    if (preparationPending) return null;
    preparationPending = true;
    selection = configuration;
    const info = await refreshSpace();
    let count = 0;
    if (configuration.formats.includes('epub'))
      for (const name of configuration.fonts) {
        const font = catalog.fonts.find(family => family.name === name);
        count += configuration.sizes === 'all' ? font.sizes.length : Array.isArray(configuration.sizes) ? configuration.sizes.length : 1;
      }
    if (configuration.orientation === 'both') count *= 2;
    $('prepare-layouts').textContent = number(count);
    $('prepare-warning-description').textContent = t(count ? 'combinationWarning' : 'fixedWarning');
    $('prepare-recommendation').hidden = !count;
    $('prepare-free').textContent = info ? bytes(info.freeBytes) : '—';
    $('prepare-reader').hidden = !configuration.formats.includes('epub') ||
      (configuration.fonts.length === 1 && configuration.fonts[0] === catalog.selectedFont && configuration.sizes === 'current');
    $('prepare-confirm').textContent = count > 1 ? t('combinations', number(count)) : t('prepareNow');
    $('prepare-space-error').hidden = !!info && info.freeBytes >= info.reserveBytes;
    $('prepare-space-error').textContent = info ? t('lowSpace') : t('recovery');
    $('prepare-confirm').disabled = !info || info.freeBytes < info.reserveBytes;
    const answer = new Promise(resolve => { resolvePreparation = resolve; });
    $('prepare-warning').showModal();
    $('prepare-back').focus();
    return answer;
  }
  $('prepare-confirm').addEventListener('click', () => finishPreparation(selection));
  $('prepare-back').addEventListener('click', () => finishPreparation(null));
  $('prepare-warning').addEventListener('cancel', event => { event.preventDefault(); finishPreparation(null); });
  $('prepare-warning').addEventListener('close', () => { if (!$('prepare-warning').open && resolvePreparation) finishPreparation(null); });
  $('prepare-reader').addEventListener('click', () => {
    window.dispatchEvent(new Event('use-reader-selection'));
    finishPreparation(null);
  });
  function setPhase(value) {
    phase = value;
    active = value === 'scanning' || value === 'deleting';
    $('cache-options').disabled = value !== 'choose';
    $('cache-options').hidden = ['preview', 'done', 'error'].includes(value);
    $('cache-date-caution').hidden = ['done', 'error'].includes(value);
    $('cache-selection').hidden = !['preview', 'done'].includes(value);
    $('cache-preview').hidden = value !== 'choose';
    $('cache-delete').hidden = value !== 'preview' || !plan?.files;
    $('cache-delete').disabled = value !== 'preview' || !$('cache-understood').checked;
    $('cache-understood').disabled = value !== 'preview';
    $('cache-review').hidden = !['preview', 'done', 'error'].includes(value);
    $('cache-close').textContent = t(value === 'scanning' ? 'cancelScan' : value === 'deleting' ? 'cancelDelete' : 'close');
    updateSpaceLabel();
  }
  function error(message) { $('cache-error').textContent = message; $('cache-error').hidden = false; }
  async function reset() {
    await window.preparer.discardCache(); plan = null;
    $('cache-summary').hidden = true; $('cache-error').hidden = true; $('cache-status').textContent = '';
    $('cache-consequence').hidden = true;
    $('cache-understood').checked = false;
    setPhase('choose');
  }
  $('cache-open').addEventListener('click', async () => {
    try { await reset(); }
    catch { error(t('recovery')); setPhase('error'); }
    $('cache-dialog').showModal(); $('cache-close').focus();
  });
  for (const choice of document.querySelectorAll('[name=cache-mode]')) choice.addEventListener('change', () => {
    $('cache-dates').hidden = choice.checked && choice.value === 'all';
  });
  $('cache-preview').addEventListener('click', async () => {
    const mode = document.querySelector('[name=cache-mode]:checked').value;
    if (mode === 'period' && (!$('cache-from').reportValidity() || !$('cache-to').reportValidity() || $('cache-from').value > $('cache-to').value)) {
      error(t('invalidDates')); return;
    }
    $('cache-error').hidden = true; $('cache-status').textContent = t('scanning'); setPhase('scanning');
    try {
      plan = await window.preparer.previewCache({mode, from: $('cache-from').value, to: $('cache-to').value});
      const dateText = value => { const [year,month,day] = value.split('-').map(Number); return new Intl.DateTimeFormat(language(),{dateStyle:'medium'}).format(new Date(year,month-1,day)); };
      $('cache-selection').textContent = mode === 'all' ? t('allCache') : t('selectedPeriod',dateText($('cache-from').value),dateText($('cache-to').value));
      $('cache-bytes').textContent = '≈ ' + bytes(plan.allocatedBytes); $('cache-count').textContent = number(plan.files);
      $('cache-validations').textContent = plan.validationFiles ? t('validations', number(plan.validationFiles)) : '';
      $('cache-summary').hidden = false;
      $('cache-status').textContent = plan.files ? t('confirm') : t('empty');
      $('cache-consequence').hidden = !plan.files;
      setPhase('preview'); $('cache-close').focus();
    } catch (caught) {
      setPhase('error');
      if (caught.message.includes('cancelled')) $('cache-status').textContent = t('scanCancelled');
      else error(t('recovery'));
    }
  });
  $('cache-understood').addEventListener('change', () => {
    $('cache-delete').disabled = phase !== 'preview' || !$('cache-understood').checked;
  });
  $('cache-delete').addEventListener('click', async () => {
    if (phase !== 'preview' || !plan?.files || !$('cache-understood').checked) return;
    $('cache-error').hidden = true; $('cache-status').textContent = t('deleting'); setPhase('deleting');
    try {
      const result = await window.preparer.deleteCache(plan.id);
      $('cache-status').textContent = t(result.cancelled ? 'cancelled' : 'deleted', number(result.deleted), bytes(result.freedBytes));
      if (result.skipped || result.failed) error(t('skipped', number(result.skipped), number(result.failed)));
      $('cache-consequence').hidden = true; setPhase('done');
      await refreshSpace();
      if (result.deleted) window.dispatchEvent(new Event('cache-cleanup-finished'));
      $('cache-close').focus();
    } catch { setPhase('error'); error(t('recovery')); }
  });
  $('cache-review').addEventListener('click', () => reset().catch(() => error(t('recovery'))));
  $('cache-close').addEventListener('click', () => {
    if (active) window.preparer.cancelCache().catch(() => error(t('recovery')));
    else $('cache-dialog').close();
  });
  $('cache-dialog').addEventListener('cancel', event => {
    if (active) { event.preventDefault(); window.preparer.cancelCache().catch(() => error(t('recovery'))); }
  });
  $('cache-dialog').addEventListener('close', () => { if (!active) window.preparer.discardCache().catch(() => {}); });
  window.preparer.onCacheEvent(event => {
    if (event.type === 'scan') $('cache-status').textContent = t('scanProgress', number(event.scanned));
    if (event.type === 'delete') $('cache-status').textContent = t('deleteProgress', number(event.deleted));
  });
  window.preparer.onEvent(event => {
    if (event.type === 'idle' || event.type === 'cardRemoved' || event.type === 'started' && event.mode === 'inspect') refreshSpace();
  });
  $('language').addEventListener('change', translate);
  window.cacheSafety = Object.freeze({confirmPreparation, refreshSpace, setBusy(value) { readerBusy = value; updateSpaceLabel(); }});
  translate();
})();
