<script src="/ui.js"></script>
<script>
function uiText(source) {
  return Object.prototype.hasOwnProperty.call(window.CrossNextUi || {}, source) ? window.CrossNextUi[source] : source;
}
function uiHtml(source) {
  return uiText(source).replace(/[&<>"']/g, function (character) {
    return {'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[character];
  });
}
// Plain text only: callers assign the result with textContent or show a native confirmation.
function uiFormat(source, values) {
  var translated = uiText(source);
  var markers = function (text) { return (text.match(/\{[a-z]+\}/g) || []).sort().join('|'); };
  if (markers(translated) !== markers(source)) translated = source;
  return translated.replace(/\{([a-z]+)\}/g, function (marker, name) {
    return Object.prototype.hasOwnProperty.call(values, name) ? String(values[name]) : marker;
  });
}
document.addEventListener('DOMContentLoaded', function () {
  document.documentElement.lang = window.CrossNextLocale || 'en';
  document.documentElement.dir = /^(ar|he|fa)(-|$)/.test(document.documentElement.lang) ? 'rtl' : 'ltr';
  document.title = uiText(document.title);
  document.querySelectorAll('[data-ui]').forEach(function (element) {
    element.childNodes.forEach(function (node) {
      if (node.nodeType !== Node.TEXT_NODE) return;
      var original = node.textContent;
      var text = original.trim();
      var translated = uiText(text);
      if (text && translated !== text) node.textContent = original.replace(text, translated);
    });
    ['title', 'aria-label', 'placeholder'].forEach(function (name) {
      if (element.hasAttribute(name)) element.setAttribute(name, uiText(element.getAttribute(name)));
    });
  });
});
</script>
<style>
[data-ui] { white-space: normal; overflow-wrap: anywhere; }
button[data-ui], a.btn[data-ui] { height: auto; min-height: 2.5em; }
.nav-links { flex-wrap: wrap; }
[dir="rtl"] [data-ui], [dir="rtl"] .band-note { letter-spacing: normal; }
input, textarea { unicode-bidi: plaintext; }
</style>
