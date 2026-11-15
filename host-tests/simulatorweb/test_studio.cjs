// Browser integration against a running local simulator, not a device latency test.
const {chromium} = require(process.env.CROSSNEXT_PLAYWRIGHT || 'playwright');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const base = process.argv[2] || 'http://127.0.0.1:8766/';
const output = process.argv[3] || path.resolve('qa-artifacts/promo-simulator/browser');
fs.mkdirSync(output, {recursive: true});
(async () => {
  const browser = await chromium.launch({headless: true, channel: process.env.CROSSNEXT_BROWSER_CHANNEL || undefined});
  const cases = [
    ['home-en', ''],
    ['pet-en', '?start=PET'],
    ['pet-adult-es', '?start=PET&language=es&species=1&age=4'],
    ['quest-en', '?start=READING+QUEST'],
    ['quest-female-fr', '?start=READING+QUEST&language=fr&gender=1&panel=shop'],
    ['quest-th', '?start=READING+QUEST&language=th'],
    ['pet-uk', '?start=PET&language=uk'],
    ['quest-ar', '?start=READING+QUEST&language=ar'],
    ['wordle-en', '?start=WORDLE'],
    ['wordle-zh', '?start=WORDLE&language=zh-CN'],
    ['focus-en', '?start=FOCUS'],
  ];
  if (process.argv.includes('--all')) {
    const catalog = await (await fetch(new URL('/showcase/catalog.json', base))).json();
    const included = new Set(cases.map(([, query]) => new URLSearchParams(query).get('start')).filter(Boolean));
    for (const title of [...catalog.apps, ...catalog.games]) {
      if (!included.has(title)) cases.push([title.toLowerCase().replace(/[^a-z0-9]/g, '-'), '?start=' + encodeURIComponent(title)]);
    }
    for (const theme of [0, 1, 2]) cases.push(['home-theme-' + theme, '?theme=' + theme]);
  }
  try {
    for (const [name, query] of cases) {
      const context = await browser.newContext({viewport: {width: 1280, height: 1150}, acceptDownloads: true});
      const page = await context.newPage();
      const errors = [];
      const messages = [];
      await page.addInitScript(() => {
        let api;
        Object.defineProperty(window, 'Crossplay', {
          configurable: true, get() { return api; }, set(value) {
            api = value;
            const boot = api.boot;
            api.boot = (mount, options) => {
              const files = options.files;
              const pet = files['/fs_/.crosspoint/pet-v2/state-a.bin'];
              window.testCard = {paths: Object.keys(files), petCreated: pet[74], age: pet[73], species: pet[60]};
              return boot(mount, options);
            };
          },
        });
      });
      page.on('pageerror', error => errors.push(String(error)));
      page.on('console', message => {
        messages.push(message.text());
        if (message.type() === 'error') errors.push(message.text());
      });
      await page.goto(base + query, {timeout: 120000});
      await page.waitForFunction(() => window.showcaseDevice && !document.getElementById('capture').disabled, null, {timeout: 120000});
      await page.waitForTimeout(2500);
      assert.equal(await page.locator('#language option').count(), 39);
      assert.equal(await page.locator('#language').inputValue(), new URLSearchParams(query).get('language') || 'en');
      assert.equal(await page.locator('#start optgroup option').count(), 31);
      assert.deepEqual(errors, [], name);
      const card = await page.evaluate(() => window.testCard);
      assert.equal(card.petCreated, 1);
      assert.equal(card.age, Number(new URLSearchParams(query).get('age') || 3));
      assert.equal(card.species, Number(new URLSearchParams(query).get('species') || 0));
      assert.ok(card.paths.some(path => path.startsWith('/fs_/languages/fonts/Interface')));
      assert.ok(card.paths.some(path => path.startsWith('/fs_/languages/wordle/') && path.endsWith('.cpwb')));
      fs.writeFileSync(path.join(output, name + '.log'), messages.join('\n'));
      const data = await page.evaluate(() => window.showcaseDevice.screenshot());
      const png = Buffer.from(data.split(',')[1], 'base64');
      const width = png.readUInt32BE(16), height = png.readUInt32BE(20);
      assert.ok((width === 480 && height === 800) || (width === 800 && height === 480));
      fs.writeFileSync(path.join(output, name + '.png'), png);
      if (name === 'home-en') {
        assert.equal(await page.locator('#language').inputValue(), 'en');
        const downloaded = page.waitForEvent('download');
        await page.locator('#capture').click();
        const download = await downloaded;
        await download.saveAs(path.join(output, 'toolbar-download.png'));
        await page.locator('#clean').click();
        assert.equal(await page.locator('body.clean').count(), 1);
        await page.keyboard.press('Escape');
        assert.equal(await page.locator('body.clean').count(), 0);
        await page.locator('#language').selectOption('es');
        await page.waitForURL(/language=es/);
        await page.waitForFunction(() => window.showcaseDevice && !document.getElementById('capture').disabled, null, {timeout: 120000});
        assert.equal(await page.locator('#language').inputValue(), 'es');
      }
      console.log('PASS', name, width + '×' + height);
      await context.close();
    }
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
