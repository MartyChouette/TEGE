/*
 * enjin-embed.js - click-to-play game embeds. "A GIF, but for videogames."
 *
 * Drop this in a post:
 *
 *   <div class="enjin-embed"
 *        data-game="/demoroom/playground/"
 *        data-title="Playground"
 *        data-poster="/posters/playground.jpg"></div>
 *   <script src="/embed/enjin-embed.js" defer></script>
 *
 * Until someone clicks, an embed costs a poster image and nothing else - no
 * engine, no wasm, no pak. A page can carry a dozen of them. On click the game
 * boots inside an iframe.
 *
 * WHY AN IFRAME
 * Emscripten hangs its runtime off a single global Module, so two players cannot
 * share a page, and there is no clean way to unload one. An iframe gives real
 * isolation and, more importantly, real teardown: removing it returns the whole
 * heap (hundreds of MB with GPU resources) to the browser. Closing a game
 * actually frees it, which is what makes many-per-page safe.
 */
(function () {
  'use strict';

  var SCRIPT = document.currentScript;
  var BASE = (function () {
    // Resolve sibling files (player.html) against wherever this script lives.
    var src = SCRIPT && SCRIPT.src ? SCRIPT.src : '';
    return src ? src.replace(/[^/]*$/, '') : '/embed/';
  })();

  var CSS = [
    '.enjin-embed{position:relative;display:block;width:100%;aspect-ratio:16/9;',
    'background:#0b0d12;border-radius:10px;overflow:hidden;cursor:pointer;',
    'font:13px/1.45 ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif;color:#e6edf3;',
    '-webkit-tap-highlight-color:transparent}',
    '.enjin-embed.is-live{cursor:default}',
    '.enjin-embed__poster{position:absolute;inset:0;width:100%;height:100%;object-fit:cover;',
    'display:block;transition:transform .35s ease,filter .35s ease}',
    '.enjin-embed:hover .enjin-embed__poster{transform:scale(1.03);filter:brightness(1.08)}',
    '.enjin-embed__fallback{position:absolute;inset:0;background:',
    'radial-gradient(120% 120% at 30% 15%,#1d2740 0%,#0b0d12 70%)}',
    '.enjin-embed__scrim{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;',
    'background:linear-gradient(180deg,rgba(0,0,0,.05),rgba(0,0,0,.45))}',
    '.enjin-embed__play{width:64px;height:64px;border-radius:50%;background:rgba(12,16,24,.72);',
    'backdrop-filter:blur(6px);border:1px solid rgba(255,255,255,.22);display:flex;align-items:center;',
    'justify-content:center;transition:transform .2s ease,background .2s ease}',
    '.enjin-embed:hover .enjin-embed__play{transform:scale(1.09);background:rgba(31,111,235,.9)}',
    '.enjin-embed__play svg{margin-left:4px}',
    '.enjin-embed__meta{position:absolute;left:12px;right:12px;bottom:10px;display:flex;',
    'align-items:baseline;gap:8px;text-shadow:0 1px 3px rgba(0,0,0,.7);pointer-events:none}',
    '.enjin-embed__title{font-weight:600}',
    '.enjin-embed__hint{font-size:11px;color:#b9c2cc}',
    '.enjin-embed__frame{position:absolute;inset:0;width:100%;height:100%;border:0;display:block}',
    '.enjin-embed__bar{position:absolute;left:0;right:0;bottom:0;height:3px;background:rgba(255,255,255,.12)}',
    '.enjin-embed__bar>i{display:block;height:100%;width:0;background:#58a6ff;transition:width .18s ease}',
    '.enjin-embed__tools{position:absolute;top:8px;right:8px;display:flex;gap:6px;opacity:0;',
    'transition:opacity .2s ease;z-index:2}',
    '.enjin-embed.is-live:hover .enjin-embed__tools,.enjin-embed.is-live:focus-within .enjin-embed__tools{opacity:1}',
    '.enjin-embed__btn{width:30px;height:30px;border-radius:7px;border:1px solid rgba(255,255,255,.2);',
    'background:rgba(12,16,24,.75);color:#e6edf3;cursor:pointer;display:flex;align-items:center;',
    'justify-content:center;padding:0}',
    '.enjin-embed__btn:hover{background:rgba(31,111,235,.9)}',
    '.enjin-embed.is-expanded{position:fixed;inset:0;width:100vw;height:100vh;',
    'aspect-ratio:auto;border-radius:0;z-index:2147483000}'
  ].join('');

  function injectCSS() {
    if (document.getElementById('enjin-embed-css')) return;
    var s = document.createElement('style');
    s.id = 'enjin-embed-css';
    s.textContent = CSS;
    document.head.appendChild(s);
  }

  var ICON_PLAY = '<svg width="22" height="22" viewBox="0 0 24 24" fill="#fff"><path d="M8 5v14l11-7z"/></svg>';
  var ICON_FULL = '<svg width="15" height="15" viewBox="0 0 20 20" fill="currentColor"><path d="M3 3h5v2H5v3H3V3zm9 0h5v5h-2V5h-3V3zM3 12h2v3h3v2H3v-5zm12 3h-3v2h5v-5h-2v3z"/></svg>';
  var ICON_CLOSE = '<svg width="15" height="15" viewBox="0 0 20 20" fill="currentColor"><path d="M5.3 4l-1.3 1.3 4.7 4.7-4.7 4.7L5.3 16l4.7-4.7 4.7 4.7 1.3-1.3-4.7-4.7 4.7-4.7L14.7 4 10 8.7 5.3 4z"/></svg>';

  // Only one game runs at a time by default: each live player holds a WebGPU
  // device and a large heap, and phones fall over well before the second one.
  var live = null;

  function teardown(el) {
    var frame = el.querySelector('.enjin-embed__frame');
    if (frame) frame.remove();          // removing the iframe frees the whole runtime
    el.classList.remove('is-live', 'is-expanded');
    var tools = el.querySelector('.enjin-embed__tools');
    if (tools) tools.remove();
    var bar = el.querySelector('.enjin-embed__bar');
    if (bar) bar.remove();
    var scrim = el.querySelector('.enjin-embed__scrim');
    if (scrim) scrim.style.display = '';
    if (live === el) live = null;
    if (document.fullscreenElement) { try { document.exitFullscreen(); } catch (e) {} }
  }

  function launch(el) {
    if (el.classList.contains('is-live')) return;
    if (live && live !== el) teardown(live);
    live = el;
    el.classList.add('is-live');

    var game = el.dataset.game || '';
    var version = el.dataset.version || '';
    var src = BASE + 'player.html?game=' + encodeURIComponent(game) +
              (version ? '&v=' + encodeURIComponent(version) : '');

    var bar = document.createElement('div');
    bar.className = 'enjin-embed__bar';
    bar.innerHTML = '<i></i>';
    el.appendChild(bar);

    var frame = document.createElement('iframe');
    frame.className = 'enjin-embed__frame';
    frame.src = src;
    frame.allow = 'fullscreen; autoplay; gamepad; xr-spatial-tracking';
    frame.setAttribute('allowfullscreen', '');
    frame.title = el.dataset.title || 'Game';
    el.appendChild(frame);

    var tools = document.createElement('div');
    tools.className = 'enjin-embed__tools';
    var full = document.createElement('button');
    full.className = 'enjin-embed__btn';
    full.title = 'Fullscreen';
    full.innerHTML = ICON_FULL;
    full.addEventListener('click', function (e) { e.stopPropagation(); toggleFull(el, frame); });
    var close = document.createElement('button');
    close.className = 'enjin-embed__btn';
    close.title = 'Close';
    close.innerHTML = ICON_CLOSE;
    close.addEventListener('click', function (e) { e.stopPropagation(); teardown(el); });
    tools.appendChild(full);
    tools.appendChild(close);
    el.appendChild(tools);

    el._enjinFrame = frame;
    el._enjinBar = bar.firstChild;
  }

  function toggleFull(el, frame) {
    // Real fullscreen where it exists; iPhone Safari refuses it for anything
    // but video, so fall back to a fixed full-viewport element.
    if (document.fullscreenElement || document.webkitFullscreenElement) {
      (document.exitFullscreen || document.webkitExitFullscreen || function () {}).call(document);
      return;
    }
    if (el.classList.contains('is-expanded')) { el.classList.remove('is-expanded'); return; }
    var fn = el.requestFullscreen || el.webkitRequestFullscreen;
    if (fn) {
      try {
        var p = fn.call(el);
        if (p && p.catch) p.catch(function () { el.classList.add('is-expanded'); });
      } catch (e) { el.classList.add('is-expanded'); }
    } else {
      el.classList.add('is-expanded');
    }
  }

  function build(el) {
    if (el._enjinBuilt) return;
    el._enjinBuilt = true;

    var poster = el.dataset.poster;
    if (poster) {
      var img = document.createElement('img');
      img.className = 'enjin-embed__poster';
      img.loading = 'lazy';
      img.decoding = 'async';
      img.alt = el.dataset.title || 'Game preview';
      img.src = poster;
      // A missing poster must not leave a broken-image box in someone's post.
      img.addEventListener('error', function () {
        img.remove();
        var fb = document.createElement('div');
        fb.className = 'enjin-embed__fallback';
        el.insertBefore(fb, el.firstChild);
      });
      el.appendChild(img);
    } else {
      var fb = document.createElement('div');
      fb.className = 'enjin-embed__fallback';
      el.appendChild(fb);
    }

    var scrim = document.createElement('div');
    scrim.className = 'enjin-embed__scrim';
    scrim.innerHTML = '<div class="enjin-embed__play">' + ICON_PLAY + '</div>';
    el.appendChild(scrim);

    var meta = document.createElement('div');
    meta.className = 'enjin-embed__meta';
    var title = el.dataset.title || '';
    var hint = el.dataset.hint || 'click to play';
    meta.innerHTML = '<span class="enjin-embed__title"></span><span class="enjin-embed__hint"></span>';
    meta.firstChild.textContent = title;
    meta.lastChild.textContent = hint;
    el.appendChild(meta);

    if (el.dataset.aspect) el.style.aspectRatio = el.dataset.aspect;

    el.setAttribute('role', 'button');
    el.setAttribute('tabindex', '0');
    el.addEventListener('click', function () { launch(el); });
    el.addEventListener('keydown', function (e) {
      if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); launch(el); }
    });
  }

  // Progress + readiness reported by the player iframe.
  addEventListener('message', function (ev) {
    if (!ev.data || !ev.data.enjin) return;
    var el = live;
    if (!el) return;
    if (ev.data.enjin === 'progress' && el._enjinBar) {
      el._enjinBar.style.width = Math.round((ev.data.frac || 0) * 100) + '%';
    } else if (ev.data.enjin === 'ready') {
      var scrim = el.querySelector('.enjin-embed__scrim');
      if (scrim) scrim.style.display = 'none';
      var bar = el.querySelector('.enjin-embed__bar');
      if (bar) bar.remove();
      var img = el.querySelector('.enjin-embed__poster');
      if (img) img.style.display = 'none';
    }
  });

  addEventListener('keydown', function (e) {
    if (e.key === 'Escape' && live) {
      if (live.classList.contains('is-expanded')) live.classList.remove('is-expanded');
      else teardown(live);
    }
  });

  function scan() {
    injectCSS();
    document.querySelectorAll('.enjin-embed').forEach(build);
  }

  if (document.readyState === 'loading') addEventListener('DOMContentLoaded', scan);
  else scan();

  window.EnjinEmbed = { scan: scan, close: function () { if (live) teardown(live); } };
})();
