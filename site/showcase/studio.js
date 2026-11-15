'use strict';
(async function () {
  const params = new URLSearchParams(location.search);
  const ids = ['language', 'theme', 'start', 'species', 'age', 'progress', 'gender', 'panel'];
  const controls = Object.fromEntries(ids.map(id => [id, document.getElementById(id)]));
  const status = document.getElementById('status');
  function choose(control, value, fallback) {
    control.value = value;
    if (control.selectedIndex < 0) control.value = fallback;
  }
  async function fetchData(path, binary = false) {
    const response = await fetch(path);
    if (!response.ok) throw new Error('Missing simulator resource: ' + path);
    return binary ? new Uint8Array(await response.arrayBuffer()) : response.json();
  }
  const decode = value => Uint8Array.from(atob(value), ch => ch.charCodeAt(0));
  function cleanView(enabled) {
    document.body.classList.toggle('clean', enabled);
    document.getElementById('restore').hidden = !enabled;
  }
  document.getElementById('clean').addEventListener('click', () => cleanView(true));
  document.getElementById('restore').addEventListener('click', () => cleanView(false));
  document.addEventListener('keydown', event => { if (event.key === 'Escape') cleanView(false); });
  document.getElementById('reset').addEventListener('click', () => location.reload());
  try {
    const catalog = await fetchData('/showcase/catalog.json');
    if (catalog.format !== 1 || !catalog.pet || !catalog.quest || !catalog.languages.length) throw new Error('Rebuild the simulator data.');
    for (const locale of catalog.languages) {
      const option = new Option(locale.name + ' · ' + locale.code, locale.code);
      option.dir = 'auto';
      controls.language.add(option);
    }
    for (const group of ['apps', 'games']) {
      const options = document.createElement('optgroup');
      options.label = group === 'apps' ? 'Apps' : 'Games';
      for (const title of catalog[group]) options.appendChild(new Option(title, title));
      controls.start.appendChild(options);
    }
    const defaults = {language: 'en', theme: '3', start: '', species: '0', age: '3', progress: '1', gender: '0', panel: 'hero'};
    for (const id of ids) {
      choose(controls[id], params.get(id), defaults[id]);
      controls[id].disabled = false;
      controls[id].addEventListener('change', () => {
        const next = new URL(location.href);
        for (const name of ids) next.searchParams.set(name, controls[name].value);
        location.href = next;
      });
    }
    const locale = catalog.languages.find(item => item.code === controls.language.value);
    const files = {};
    await Promise.all(locale.files.map(async file => { files['/fs_/languages/' + file] = await fetchData('/showcase/languages/' + file, true); }));
    if (locale.pack) files['/fs_/languages/' + locale.pack] = await fetchData('/showcase/languages/' + locale.pack, true);
    if (locale.wordle) files['/fs_/languages/wordle/' + locale.wordle] = await fetchData('/showcase/wordle/' + locale.wordle, true);
    const settings = {...catalog.settings, language: locale.setting, languagePack: locale.pack || '', homeLayout: Number(controls.theme.value)};
    files['/fs_/.crosspoint/settings.json'] = JSON.stringify(settings);
    files['/fs_/.crosspoint/state.json'] = JSON.stringify({openEpubPath: '', lastSleepFromReader: false, showBootScreen: false});
    files['/fs_' + catalog.paths.pet] = decode(catalog.pet[`pet-${controls.species.value}-${controls.age.value}`]);
    files['/fs_' + catalog.paths.petHistory] = decode(catalog.petHistory);
    files['/fs_' + catalog.paths.quest] = decode(catalog.quest[`quest-${controls.progress.value}-${controls.gender.value}`]);
    const inventory = catalog.inventory;
    document.getElementById('inventory').textContent = `${inventory.books} books · ${catalog.apps.length} apps · ${catalog.games.length} games · ${catalog.languages.length} languages · 120 days of sample reading. Demo date: ${new Date(catalog.epoch * 1000).toISOString().slice(0, 10)} (UTC).`;
    status.textContent = 'Loading firmware and bundled books…';
    const device = await Crossplay.boot(document.getElementById('device'), {
      files,
      wipe: [catalog.paths.pet, catalog.paths.quest].map(path => '/fs_' + path.slice(0, path.lastIndexOf('/'))),
      env: {CROSSPLAY_AUTOSTART: controls.start.value, CROSSNEXT_SIM_EPOCH: String(catalog.epoch), CROSSNEXT_QUEST_PANEL: controls.panel.value},
      onFirstFrame() {
        status.textContent = 'Firmware running · ' + locale.name;
        document.getElementById('capture').disabled = false;
      },
      onRestart(snapshot) {
        const next = new URL(location.href);
        try {
          const saved = JSON.parse(snapshot.settings);
          const selected = catalog.languages.find(item => item.pack && item.pack === saved.languagePack)
            || catalog.languages.find(item => !item.pack && item.setting === saved.language);
          if (selected) next.searchParams.set('language', selected.code);
          if ([0, 1, 2, 3].includes(saved.homeLayout)) next.searchParams.set('theme', saved.homeLayout);
        } catch (error) { console.warn('Could not retain the simulator language on restart.', error); }
        location.replace(next);
      },
    });
    // Exposed for local automated screenshot checks as well as toolbar controls.
    window.showcaseDevice = device;
    for (const key of ['up', 'down']) {
      const button = document.getElementById(key);
      button.disabled = false;
      device.bindButton(button, device.scancodes[key]);
    }
    const back = document.getElementById('back');
    back.disabled = false;
    back.addEventListener('click', () => device.swipeBack());
    document.getElementById('capture').addEventListener('click', () => {
      const download = document.createElement('a');
      download.href = device.screenshot();
      download.download = 'CrossNext-' + (controls.start.value || 'Home').replace(/[^a-z0-9-]/gi, '-') + '-' + locale.code + '-' + Date.now() + '.png';
      download.click();
    });
    device.canvas.focus({preventScroll: true});
  } catch (error) {
    status.textContent = 'Could not start: ' + error.message;
    console.error(error);
  }
})();
