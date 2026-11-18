'use strict';
(() => {
  const main = document.getElementById('main');
  const list = document.getElementById('font-list');
  let pending = false;

  function fit() {
    pending = false;
    const mainStyle = getComputedStyle(main);
    const compactHeight = parseFloat(getComputedStyle(list).getPropertyValue('--compact-height'));
    // Add the scroll offset so scrolling cannot change the available layout space.
    const top = list.getBoundingClientRect().top - main.getBoundingClientRect().top - main.clientTop + main.scrollTop;
    const height = Math.max(compactHeight, Math.floor(main.clientHeight - top - parseFloat(mainStyle.paddingBottom)));
    const value = height + 'px';
    if (list.style.maxHeight !== value) list.style.maxHeight = value;
  }

  function schedule() {
    if (pending) return;
    pending = true;
    requestAnimationFrame(fit);
  }

  const observer = new ResizeObserver(schedule);
  for (const node of [main, ...document.querySelectorAll('.intro,.card-section,.options-section>.section-heading,.formats,.font-column>.label-row,.font-actions,.preparation-footer')]) {
    observer.observe(node);
  }
  window.addEventListener('resize', schedule);
  list.addEventListener('focusin', event => {
    const row = event.target.closest('label');
    if (!row) return;
    const bounds = row.getBoundingClientRect(), area = list.getBoundingClientRect();
    const top = area.top + list.clientTop, bottom = top + list.clientHeight;
    if (bounds.top < top) list.scrollTop -= top - bounds.top;
    else if (bounds.bottom > bottom) list.scrollTop += bounds.bottom - bottom;
  });
  schedule();
})();
