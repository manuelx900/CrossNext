// JavaScript inside EM_JS must retain JavaScript operators.
// clang-format off
#include <emscripten.h>
EM_JS(int, test_http_request, (const char* url, const char* headers, const char* method, const char* body), {
  const args = [ '--silent', '--show-error', '--max-time', '5', '--include' ];
  for (const h of UTF8ToString(headers).split('\n'))
    if (h) args.push('--header', h);
  args.push('--request', UTF8ToString(method));
  if (body) args.push('--data-binary', UTF8ToString(body));
  args.push(UTF8ToString(url));
  const p = require('child_process').spawnSync('curl', args, {encoding : null, maxBuffer : 4 * 1024 * 1024});
  const output = p.stdout || Buffer.alloc(0);
  const end = output.indexOf('\r\n\r\n');
  globalThis.syncResponse = end < 0 ? Buffer.alloc(0) : output.subarray(end + 4);
  const status = end < 0 ? -1 : parseInt(output.subarray(0, end).toString().split(' ')[1]);
  globalThis.syncComplete = p.status === 0;
  return Number.isFinite(status) ? status : -1;
});
EM_JS(int, test_http_size, (), { return globalThis.syncResponse.length; });
EM_JS(int, test_http_complete, (), { return globalThis.syncComplete ? 1 : 0; });
EM_JS(void, test_http_copy, (unsigned char* out, int at, int size),
      { HEAPU8.set(globalThis.syncResponse.subarray(at, at + size), out); });

// clang-format on
