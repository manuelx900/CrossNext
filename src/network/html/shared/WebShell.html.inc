<style>
  :root {
    --bg: #f6f7f9; --card-bg: #fff; --font-color: #16181d; --title-color: #16181d;
    --label-color: #59616e; --border-color: #dce0e6; --accent-color: #0866d9;
    --accent-hover-color: #004da8; --accent-color-10: #e9f1ff;
    --folder-color: #9a6800; --danger-color: #c32929; --sheet-scrim: #0008;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --bg: #121315; --card-bg: #1d1f23; --font-color: #f1f2f4; --title-color: #f1f2f4;
      --label-color: #a9afb9; --border-color: #33363d; --accent-color: #78b7ff;
      --accent-hover-color: #b5d7ff; --accent-color-10: #223047;
      --folder-color: #ffd35a; --danger-color: #ff7777;
      --plugin-control-bg: #1d1f23; --plugin-control-border: #555c68; color-scheme: dark;
    }
  }
  * { box-sizing: border-box; }
  body { max-width: 1100px; margin: 0 auto; padding: 0 28px 110px; font-size: 16px; line-height: 1.45; background: var(--bg); color: var(--font-color); }
  body > h1 { color: var(--title-color); font-size: 20px; border: 0; margin: 0; padding: 23px 0 14px; letter-spacing: -.4px; }
  .nav-links { display: flex; flex-wrap: wrap; gap: 6px; border-bottom: 1px solid var(--border-color); padding-bottom: 12px; }
  .nav-links a { border-radius: 8px; padding: 10px 14px; color: var(--label-color); text-decoration: none; min-height: 44px; }
  .nav-links a.active { background: var(--accent-color-10); color: var(--accent-color); }
  .card { border-radius: 12px; box-shadow: none; border: 1px solid var(--border-color); }
  button, input, select, textarea { font-size: 16px; }
  button { cursor: pointer; }
  a:focus-visible, button:focus-visible, input:focus-visible, select:focus-visible, textarea:focus-visible { outline: 3px solid var(--accent-color); outline-offset: 3px; }
  .fm-tabs { display: none; }
  @media (max-width: 640px) {
    body { padding: 0 16px calc(100px + env(safe-area-inset-bottom)); }
    body[data-standalone] { padding-bottom: calc(20px + env(safe-area-inset-bottom)); }
    body > h1 { font-size: 18px; text-align: center; padding: 18px 16px; border-bottom: 1px solid var(--border-color); margin-inline: -16px; }
    body > .nav-links { display: none; }
    .card { padding: 16px; }
    .fm-tabs { position: fixed; display: flex; bottom: 0; left: 0; right: 0; z-index: 30; justify-content: space-around; padding: 9px 8px calc(9px + env(safe-area-inset-bottom)); border-top: 1px solid var(--border-color); background: var(--bg); }
    .fm-tabs a { display: flex; flex-direction: column; align-items: center; gap: 3px; color: var(--label-color); text-decoration: none; font-size: 11px; min-height: 44px; min-width: 64px; }
    .fm-tabs a[aria-current] { color: var(--accent-color); }
    .fm-tabs svg { width: 23px; height: 23px; }
  }
  @media (prefers-reduced-motion: reduce) { *, *::before, *::after { scroll-behavior: auto !important; transition: none !important; animation: none !important; } }
</style>
<script>
document.addEventListener('DOMContentLoaded', function() {
  if (document.body.hasAttribute('data-standalone')) return;
  const nav = document.createElement('nav'); nav.className = 'fm-tabs'; nav.setAttribute('aria-label','CrossNext');
  const paths = ['/files','/settings','/fonts','/'];
  const labels = [uiText('Library'),uiText('Settings'),uiText('Fonts'),uiText('Home')];
  const shapes = ['<path d="M5 3h15l-3 16H4a2 2 0 0 1 0-4h14M5 3 3 16M8 7h8M7 11h8"/>',
    '<path d="M3 6h18M3 18h18"/><circle cx="8" cy="6" r="3"/><circle cx="16" cy="18" r="3"/>',
    '<path d="M4 4h16M12 4v16M8 20h8"/>', '<path d="m3 10 9-7 9 7M5 9v12h14V9M10 21v-7h4v7"/>'];
  paths.forEach(function(path,index) {
    const link=document.createElement('a');link.href=path;
    if (location.pathname===path) link.setAttribute('aria-current','page');
    link.innerHTML='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'+shapes[index]+'</svg>';
    const text=document.createElement('span');text.textContent=labels[index];link.appendChild(text);nav.appendChild(link);
  });
  document.body.appendChild(nav);
});
</script>
