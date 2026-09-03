/*
 * enjin-boot.js - gzip-aware WebAssembly loader for the Enjin web player.
 *
 * WHY THIS EXISTS
 * The player wasm is ~11.6 MB raw and ~3.2 MB gzipped, but a static host that
 * does not set Content-Encoding on application/wasm sends the whole 11.6 MB to
 * every visitor. (marty64.net was doing exactly that: Apache, no compression,
 * Content-Length 12121999.) Fixing the server is the better answer where you
 * control it - see enjin.htaccess - but this makes the win independent of host
 * configuration, which matters for an embed other people paste into their pages.
 *
 * HOW
 * Fetch EnjinPlayer.wasm.gz, inflate it with the browser's native
 * DecompressionStream, and hand the bytes to Emscripten through Module.wasmBinary
 * (a documented hook: when set, the glue skips its own fetch entirely). No extra
 * decoder ships with the page. Brotli would be ~2.2 MB but DecompressionStream
 * has no brotli, and a JS brotli decoder costs more than the ~1 MB it saves.
 *
 * Falls back to the plain .wasm whenever anything is missing: no .gz on the
 * server, no DecompressionStream (Safari < 16.4), or a decompression error. The
 * game always boots; only the transfer size changes.
 */
(function (global) {
  'use strict';

  var GZIP_SUPPORTED = typeof global.DecompressionStream === 'function';

  function fmtMB(bytes) { return (bytes / 1048576).toFixed(1) + ' MB'; }

  // Fetch with progress. Content-Length is the COMPRESSED size when we ask for
  // the .gz directly, which is what we want to show the user.
  function fetchWithProgress(url, onProgress) {
    return fetch(url).then(function (res) {
      if (!res.ok) throw new Error(url + ' -> HTTP ' + res.status);
      var total = parseInt(res.headers.get('content-length') || '0', 10);
      if (!res.body || !total) return res.arrayBuffer();   // no streaming info: just wait
      var reader = res.body.getReader();
      var chunks = [], received = 0;
      return (function pump() {
        return reader.read().then(function (r) {
          if (r.done) {
            var out = new Uint8Array(received), at = 0;
            for (var i = 0; i < chunks.length; i++) { out.set(chunks[i], at); at += chunks[i].length; }
            return out.buffer;
          }
          chunks.push(r.value);
          received += r.value.length;
          if (onProgress) onProgress(received, total);
          return pump();
        });
      })();
    });
  }

  function inflate(buffer) {
    var ds = new global.DecompressionStream('gzip');
    var stream = new Response(buffer).body.pipeThrough(ds);
    return new Response(stream).arrayBuffer();
  }

  /**
   * Load the player wasm, preferring the gzipped copy.
   * @param {string} baseUrl  directory holding EnjinPlayer.js/.wasm[.gz]
   * @param {object} opts     { version, onProgress(frac, label), onError }
   * @returns {Promise<ArrayBuffer|null>} bytes for Module.wasmBinary, or null to
   *          let Emscripten fetch the .wasm itself.
   */
  function loadWasmBinary(baseUrl, opts) {
    opts = opts || {};
    var v = opts.version ? ('?v=' + opts.version) : '';
    var gzUrl = baseUrl + 'EnjinPlayer.wasm.gz' + v;
    var report = function (frac, label) { if (opts.onProgress) opts.onProgress(frac, label); };

    if (!GZIP_SUPPORTED) {
      report(0, 'downloading engine');
      return Promise.resolve(null);          // browser can't inflate; use the plain path
    }

    return fetchWithProgress(gzUrl, function (got, total) {
      // Downloading is most of the wait; leave the last 10% for inflate.
      report(0.9 * (got / total), 'downloading engine ' + fmtMB(got) + ' / ' + fmtMB(total));
    }).then(function (buf) {
      report(0.92, 'unpacking engine');
      return inflate(buf);
    }).then(function (wasm) {
      report(1.0, 'starting');
      return wasm;
    }).catch(function (err) {
      // Any problem at all: fall back to the uncompressed wasm.
      if (global.console && console.warn) console.warn('[enjin-boot] gz path unavailable, using .wasm:', err.message);
      report(0, 'downloading engine');
      return null;
    });
  }

  /**
   * Boot the player into a canvas.
   * @param {object} cfg { baseUrl, canvas, version, onProgress, onReady, onPrint, maxPixels }
   * @returns {Promise<object>} the Emscripten Module
   */
  function boot(cfg) {
    var baseUrl = cfg.baseUrl;
    if (baseUrl && baseUrl.charAt(baseUrl.length - 1) !== '/') baseUrl += '/';
    var v = cfg.version ? ('?v=' + cfg.version) : '';
    var canvas = cfg.canvas;

    return loadWasmBinary(baseUrl, {
      version: cfg.version,
      onProgress: cfg.onProgress
    }).then(function (wasmBinary) {
      var Module = {
        // Every asset (including the .wasm fallback) resolves against the game dir.
        locateFile: function (path) { return baseUrl + path + v; },
        canvas: canvas,
        onRuntimeInitialized: function () { if (cfg.onReady) cfg.onReady(Module); },
        print: function (t) { if (cfg.onPrint) cfg.onPrint(t, false); },
        printErr: function (t) { if (cfg.onPrint) cfg.onPrint(t, true); }
      };
      if (wasmBinary) Module.wasmBinary = wasmBinary;
      global.Module = Module;

      // WebGPU needs the context requested before the runtime starts.
      if (global.navigator && global.navigator.gpu) {
        try { canvas.getContext('webgpu'); } catch (e) {}
      }

      // Backing-store cap: raw devicePixelRatio on a dpr-3 phone allocates ~9x
      // the pixels and is a known cause of "Not enough memory left" on heavier
      // scenes. Cap the long edge instead.
      var maxPixels = cfg.maxPixels || 1280;
      function sync() {
        var host = canvas.parentElement || canvas;
        var w = Math.floor(host.clientWidth), h = Math.floor(host.clientHeight);
        if (w < 64 || h < 64) return;   // collapsed layout: do not commit it
        var dpr = global.devicePixelRatio || 1;
        // Allow eff BELOW 1: flooring it at 1 meant any container wider than
        // maxPixels rendered at full resolution and the cap did nothing.
        var eff = Math.max(0.55, Math.min(dpr, maxPixels / Math.max(w, h)));
        canvas.width = Math.floor(w * eff);
        canvas.height = Math.floor(h * eff);
        if (Module._onCanvasResize) { try { Module._onCanvasResize(w, h, eff); } catch (e) {} }
      }
      Module.enjinSyncCanvas = sync;
      if (global.ResizeObserver) new ResizeObserver(sync).observe(canvas.parentElement || canvas);
      sync();

      return new Promise(function (resolve, reject) {
        var s = document.createElement('script');
        s.src = baseUrl + 'EnjinPlayer.js' + v;
        s.onload = function () { resolve(Module); };
        s.onerror = function () { reject(new Error('failed to load EnjinPlayer.js')); };
        document.head.appendChild(s);
      });
    });
  }

  global.EnjinBoot = {
    boot: boot,
    loadWasmBinary: loadWasmBinary,
    gzipSupported: GZIP_SUPPORTED
  };
})(window);
