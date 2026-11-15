'use strict';
const {_electron: electron} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const root = path.join(__dirname, '..');
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'CrossNext-locale-'));
const output = path.join(root, 'build', 'locale-qa');
fs.mkdirSync(output, {recursive: true});
let app;
const launch = () => electron.launch({args: [root, '--lang=es', '--user-data-dir=' + profile, '--disable-card-discovery']});
(async () => {
  app = await launch();
  let page = await app.firstWindow();
  await page.waitForFunction(() => document.documentElement.lang === 'en');
  assert.equal(await page.locator('#language').inputValue(), 'en');
  await page.locator('#language').selectOption('es');
  await app.close();
  app = await launch();
  page = await app.firstWindow();
  await page.waitForFunction(() => document.documentElement.lang === 'es');
  await page.locator('#language').click();
  await page.locator('#language option[value=en]').hover();
  const layout = await page.locator('#language option').evaluateAll(nodes => {
    const boxes = nodes.map(node => node.getBoundingClientRect());
    return {gap: boxes[1].top - boxes[0].bottom, separator: getComputedStyle(nodes[1]).borderTopWidth};
  });
  assert.ok(layout.gap >= 0.9 && layout.gap <= 1.1, JSON.stringify(layout));
  assert.equal(layout.separator, '0px');
  const clip=await page.locator('#language option').evaluateAll(nodes=>{
    const first=nodes[0].getBoundingClientRect(),last=nodes[nodes.length-1].getBoundingClientRect();
    const x=Math.max(0,first.x-12);
    return {x,y:0,width:Math.min(innerWidth-x,first.width+24),height:last.bottom+14};
  });
  await page.screenshot({path: path.join(output, 'separated-options.png'),clip});
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('select').evaluateAll(nodes=>nodes.every(node=>getComputedStyle(node,'::picker(select)').display==='none')),true);
  console.log('PASS English initial language under Spanish locale, saved Spanish choice, and separated hovered/selected menu rows');
})().catch(error => {console.error(error);process.exitCode = 1;}).finally(async () => {
  if (app) {await app.evaluate(({app}) => app.exit(0)).catch(() => {});await app.close().catch(() => {});}
});
