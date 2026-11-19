'use strict';
// Electron utility processes do not use Node's require.main entry-point contract.
const fs = require('node:fs');
const worker = require('./worker.cjs');
const mode = process.argv[process.argv.length - 2];
const filename = process.argv[process.argv.length - 1];
Promise.resolve().then(() => worker.main(JSON.parse(fs.readFileSync(filename, 'utf8')), mode))
  .then(() => process.stdout.write('', () => process.exit(0)))
  .catch(error => {
    process.stdout.write(JSON.stringify({type: 'error', code: error.code, book: error.book, message: String(error.message || error)}) + '\n', () => process.exit(1));
  });
