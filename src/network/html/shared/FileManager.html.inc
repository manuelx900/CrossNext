<style>
  body { max-width: 1100px; padding: 0 28px 120px; font-size: 16px; line-height: 1.45; }
  body > h1 { font-size: 20px; border: 0; margin: 0; padding: 23px 0 14px; letter-spacing: -.4px; }
  .nav-links { display: flex; gap: 6px; border-bottom: 1px solid var(--border-color); padding-bottom: 12px; }
  .nav-links a { border-radius: 8px; padding: 10px 14px; color: var(--label-color); text-decoration: none; }
  .nav-links a.active { background: var(--accent-color-10); color: var(--accent-color); }
  .page-header { margin: 28px 0 20px; gap: 16px; align-items: center; }
  .page-header h2 { font-size: 24px; letter-spacing: -.5px; }
  .action-buttons { gap: 8px; flex-wrap: wrap; }
  .action-btn { font: inherit; min-height: 44px; border-radius: 9px; padding: 10px 14px; }
  .upload-action-btn { background: #176be5; color: #fff; }
  .folder-action-btn, .selection-btn { background: var(--card-bg); color: var(--font-color); border: 1px solid var(--border-color); }
  .delete-action-btn { background: var(--danger-color); color: var(--bg); display: none; }
  body.selecting .delete-action-btn { display: inline-flex; }
  .card { border-radius: 12px; box-shadow: none; border: 1px solid var(--border-color); padding: 20px; }
  .web-footer { border: 0; background: transparent; font-size: 13px; }
  .web-footer p { color: var(--label-color) !important; }
  .files-card { padding: 0; overflow: hidden; }
  .contents-header { gap: 8px; padding: 16px 20px; margin: 0; border-bottom: 1px solid var(--border-color); }
  .breadcrumb-inline { min-width: 0; display: flex; flex-wrap: wrap; gap: 6px; font-size: 15px; }
  .breadcrumb-inline a, .breadcrumb-inline .current { overflow-wrap: anywhere; }
  .summary-inline { color: var(--label-color); font-size: 13px; white-space: normal; }
  .file-row { display: flex; align-items: center; gap: 12px; padding: 9px 12px 9px 20px; min-height: 72px; border-bottom: 1px solid var(--border-color); }
  .file-row:last-child { border-bottom: 0; }
  .file-row:hover { background: var(--accent-color-10); }
  .file-name { display: flex; align-items: center; gap: 14px; min-width: 0; flex: 1; color: var(--font-color); text-decoration: none; }
  .file-name span { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .file-name svg { width: 27px; height: 27px; flex: 0 0 27px; color: var(--accent-color); }
  .folder-row .file-name svg { color: var(--folder-color); }
  .file-size { flex: 0 0 auto; font-size: 13px; color: var(--label-color); white-space: nowrap; font-variant-numeric: tabular-nums; }
  .file-more, .selection-btn { display: inline-flex; align-items: center; justify-content: center; }
  .file-more { flex: 0 0 44px; width: 44px; height: 44px; color: var(--label-color); background: none; border: 0; border-radius: 8px; }
  .file-more svg { width: 22px; height: 22px; }
  .select-item { display: none; flex: 0 0 22px; width: 22px; height: 22px; accent-color: #176be5; }
  .selection-all { display: none; padding: 12px 20px; gap: 12px; align-items: center; border-bottom: 1px solid var(--border-color); min-height: 48px; }
  .selection-all input { width: 22px; height: 22px; accent-color: #176be5; }
  body.selecting .select-item { display: block; }
  body.selecting .selection-all { display: flex; }
  .file-row:has(.select-item:checked) { background: var(--accent-color-10); }
  .no-files { color: var(--label-color); padding: 56px 20px; text-align: center; }
  .fm-dialog { color: var(--font-color); background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 18px; padding: 22px; width: min(480px, calc(100% - 32px)); max-height: 85dvh; overflow-y: auto; }
  .fm-dialog::backdrop { background: var(--sheet-scrim); }
  .fm-dialog h3 { margin: 0 40px 4px 0; overflow-wrap: anywhere; font-size: 20px; }
  .fm-dialog p { margin: 0 0 18px; color: var(--label-color); font-size: 14px; }
  .fm-close, .fm-add { -webkit-appearance: none; appearance: none; box-sizing: border-box; padding: 0; line-height: 1; }
  .fm-close[data-ui] { position: absolute; right: 12px; top: 12px; display: flex; align-items: center; justify-content: center; width: 44px; height: 44px; min-width: 44px; min-height: 44px; max-width: 44px; max-height: 44px; border-radius: 50%; background: none; border: 0; color: var(--label-color); font-size: 26px; }
  .fm-dialog h3:focus { outline: none; }
  .fm-action { display: flex; align-items: center; gap: 16px; width: 100%; min-height: 56px; padding: 12px 4px; color: var(--font-color); background: none; border: 0; border-top: 1px solid var(--border-color); font: inherit; text-align: start; text-decoration: none; }
  .fm-action svg { width: 23px; height: 23px; flex: 0 0 23px; color: var(--label-color); }
  .fm-action.danger, .fm-action.danger svg { color: var(--danger-color); }
  .fm-add { display: none; }
  button, input, select, textarea { font-size: 16px; }
  button { cursor: pointer; }
  .modal-overlay { padding: 16px; }
  .modal { width: min(720px, 100%); max-width: 100%; max-height: 90dvh; overflow-y: auto; border: 1px solid var(--border-color); border-radius: 16px; }
  .modal h3 { padding-right: 36px; font-size: 20px; }
  .modal-close { min-width: 44px; min-height: 44px; }
  .modal input[type=text], .modal input[type=password] { max-width: 100%; min-height: 44px; font-size: 16px; }
  .modal button { min-height: 44px; }
  #notification { overflow-wrap: anywhere; }
  @media (max-width: 640px) {
    body { padding: 0 0 calc(100px + env(safe-area-inset-bottom)); }
    body > h1 { margin-inline: 0; font-size: 18px; text-align: center; padding: 18px 16px; border-bottom: 1px solid var(--border-color); }
    body > .nav-links, .web-footer { display: none; }
    .page-header { margin: 20px 16px 14px; }
    .page-header h2 { font-size: 22px; }
    .page-header .upload-action-btn, .page-header .folder-action-btn { display: none; }
    .selection-btn { font-size: 14px; padding: 8px 12px; }
    .card { margin: 14px 12px; padding: 16px; }
    .files-card { margin: 0; border: 0; border-radius: 0; padding: 0; background: transparent; }
    .contents-header { padding: 12px 16px; display: flex; flex-direction: column; align-items: stretch; }
    .summary-inline { text-align: start; }
    .file-row { padding-inline: 16px 6px; gap: 8px; }
    .file-name { gap: 12px; font-size: 17px; }
    .file-size { font-size: 12px; }
    .fm-add[data-ui] { display: flex; position: fixed; bottom: calc(88px + env(safe-area-inset-bottom)); right: 20px; z-index: 31; align-items: center; justify-content: center; width: 58px; height: 58px; min-width: 58px; min-height: 58px; max-width: 58px; max-height: 58px; aspect-ratio: 1; border-radius: 50%; border: 0; background: #176be5; color: #fff; font-size: 34px; box-shadow: 0 4px 16px #0003; }
    .fm-dialog { margin: auto 0 0; width: 100%; max-width: none; border-radius: 22px 22px 0 0; padding: 24px 22px calc(24px + env(safe-area-inset-bottom)); }
    .modal { padding: 20px 16px; }
    .modal.picker-mode { max-width: 100%; }
    #plugin-container { margin: 20px 12px; }
  }
  @media (prefers-reduced-motion: reduce) { *, *::before, *::after { scroll-behavior: auto !important; transition: none !important; animation: none !important; } }
</style>
<dialog class="fm-dialog" id="fileActions" aria-labelledby="fileActionsName">
  <button class="fm-close" type="button" onclick="document.getElementById('fileActions').close()" aria-label="Close" data-ui>&times;</button>
  <h3 id="fileActionsName" tabindex="-1" autofocus></h3><p id="fileActionsSize"></p><div id="fileActionsItems"></div>
</dialog>
<dialog class="fm-dialog" id="addFiles" aria-labelledby="addFilesTitle">
  <button class="fm-close" type="button" onclick="document.getElementById('addFiles').close()" aria-label="Close" data-ui>&times;</button>
  <h3 id="addFilesTitle" tabindex="-1" autofocus data-ui>Add files</h3>
  <button class="fm-action" onclick="document.getElementById('addFiles').close(); openUploadModal()" data-ui>📤 Upload</button>
  <button class="fm-action" onclick="document.getElementById('addFiles').close(); openFolderModal()" data-ui>📁 New Folder</button>
</dialog>
<button class="fm-add" type="button" onclick="document.getElementById('addFiles').showModal()" aria-label="Add files" data-ui>+</button>

<script>
function fileManagerIcon(name) {
  const shapes = {
    folder: '<path d="M3 6a2 2 0 0 1 2-2h5l2 3h7a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2Z"/>',
    book: '<path d="M5 3h15l-3 16H4a2 2 0 0 1 0-4h14M5 3 3 16M8 7h8M7 11h8"/>',
    file: '<path d="M5 3h9l5 5v13H5ZM14 3v6h5M8 13h8M8 17h6"/>',
    more: '<circle cx="12" cy="5" r="1"/><circle cx="12" cy="12" r="1"/><circle cx="12" cy="19" r="1"/>',
    download: '<path d="M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5"/>',
    move: '<path d="M3 6h7l2 3h9v12H3ZM9 14h9m-3-3 3 3-3 3"/>',
    rename: '<path d="m4 16-1 5 5-1L20 8l-4-4ZM13 7l4 4"/>',
    delete: '<path d="M4 6h16M9 6V3h6v3M6 6l1 15h10l1-15M10 10v7m4-7v7"/>',
    settings: '<path d="M3 6h18M3 18h18"/><circle cx="8" cy="6" r="3"/><circle cx="16" cy="18" r="3"/>',
    type: '<path d="M4 4h16M12 4v16M8 20h8"/>',
    home: '<path d="m3 10 9-7 9 7M5 9v12h14V9M10 21v-7h4v7"/>'
  };
  return '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + (shapes[name] || shapes.file) + '</svg>';
}
function showFileActions(button) {
  const name = button.dataset.name, path = button.dataset.path, folder = button.dataset.folder === 'true';
  const dialog = document.getElementById('fileActions');
  document.getElementById('fileActionsName').textContent = name;
  document.getElementById('fileActionsSize').textContent = button.dataset.size || '';
  const list = document.getElementById('fileActionsItems');
  list.replaceChildren();
  function action(icon, label, run, danger) {
    const item = document.createElement('button');
    item.type = 'button'; item.className = 'fm-action' + (danger ? ' danger' : '');
    item.innerHTML = fileManagerIcon(icon);
    item.appendChild(document.createTextNode(label));
    item.onclick = function() { dialog.close(); run(); };
    list.appendChild(item);
  }
  if (!folder) {
    const download = document.createElement('a');
    download.className = 'fm-action'; download.href = downloadUrl(path); download.download = name;
    download.innerHTML = fileManagerIcon('download'); download.appendChild(document.createTextNode(uiText('Download')));
    list.appendChild(download);
    action('move', uiText('Move file'), function() { openMoveModal(name, path); });
    action('rename', uiText('Rename file'), function() { openRenameModal(name, path); });
  }
  action('delete', uiText('Delete'), function() { openDeleteModal(name, path, folder); }, true);
  dialog.showModal();
}
function toggleFileSelection() {
  const selecting = document.body.classList.toggle('selecting');
  document.getElementById('selectFilesButton').setAttribute('aria-pressed', selecting);
  if (!selecting) document.querySelectorAll('.select-item, #selectAllCheckbox').forEach(function(input) { input.checked = false; });
}
document.addEventListener('DOMContentLoaded', function() {
  document.querySelectorAll('[data-fm-icon]').forEach(function(node) { node.innerHTML = fileManagerIcon(node.dataset.fmIcon); });
  document.querySelectorAll('.fm-dialog').forEach(function(dialog) {
    dialog.addEventListener('click', function(event) { if (event.target === dialog) { const r = dialog.getBoundingClientRect(); if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) dialog.close(); } });
  });
  // Existing transfer dialogs retain their cleanup paths. Focus follows whichever
  // overlay is open, including overlays opened by plugin code.
  const overlays = Array.from(document.querySelectorAll('.modal-overlay'));
  const returns = new Map();
  overlays.forEach(function(overlay) {
    overlay.setAttribute('role', 'dialog'); overlay.setAttribute('aria-modal', 'true'); overlay.tabIndex = -1;
    const heading = overlay.querySelector('h3');
    if (heading) { heading.id = heading.id || overlay.id + 'Title'; overlay.setAttribute('aria-labelledby', heading.id); }
    new MutationObserver(function() {
      if (overlay.classList.contains('open') && !returns.has(overlay)) {
        returns.set(overlay, document.activeElement);
        (overlay.querySelector('input:not([type=hidden]), button:not([disabled])') || overlay).focus();
      } else if (!overlay.classList.contains('open') && returns.has(overlay)) {
        const previous = returns.get(overlay); returns.delete(overlay);
        if (previous && previous.isConnected) previous.focus();
      }
    }).observe(overlay, {attributes: true, attributeFilter: ['class']});
  });
  document.addEventListener('keydown', function(event) {
    const overlay = overlays.findLast(function(node) { return node.classList.contains('open'); });
    if (!overlay) return;
    if (event.key === 'Escape') { const close = overlay.querySelector('.modal-close:not(.disabled)'); if (close) close.click(); event.preventDefault(); }
    if (event.key !== 'Tab') return;
    const nodes = Array.from(overlay.querySelectorAll('button, a[href], input, select, textarea, [tabindex="0"]')).filter(function(node) { return !node.disabled && node.getClientRects().length; });
    if (!nodes.length) { overlay.focus(); event.preventDefault(); return; }
    if (event.shiftKey && (document.activeElement === nodes[0] || !overlay.contains(document.activeElement))) { nodes[nodes.length - 1].focus(); event.preventDefault(); }
    else if (!event.shiftKey && (document.activeElement === nodes[nodes.length - 1] || !overlay.contains(document.activeElement))) { nodes[0].focus(); event.preventDefault(); }
  });
});
</script>
