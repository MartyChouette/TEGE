# Enjin web embeds

Two independent pieces. Either is useful on its own.

## 1. `enjin-boot.js` — stop shipping 11.6 MB

`EnjinPlayer.wasm` is 11.56 MB raw and 3.22 MB gzipped. A host that does not set
`Content-Encoding` on `application/wasm` sends the whole 11.56 MB to every
visitor — which is exactly what was happening:

```
$ curl -sI -H 'Accept-Encoding: br, gzip' https://marty64.net/demoroom/playground/EnjinPlayer.wasm
HTTP/1.1 200 OK
Content-Length: 12121999      <- no content-encoding at all
Server: Apache
```

`enjin-boot.js` fetches `EnjinPlayer.wasm.gz`, inflates it with the browser's
native `DecompressionStream`, and hands the bytes to Emscripten via
`Module.wasmBinary`. Measured in headless Chrome: **12,121,999 bytes → 3,372,538**,
the plain `.wasm` never requested, engine boots normally.

Brotli would be 2.22 MB, but `DecompressionStream` has no brotli and a JS decoder
costs more than the megabyte it saves.

Generate the companion file on deploy:

```bash
gzip -9 -c EnjinPlayer.wasm > EnjinPlayer.wasm.gz
```

If you control the server, `enjin.htaccess` is the better fix — it compresses
everything, not just the engine. The two are complementary; the loader falls back
to the plain `.wasm` whenever the `.gz` or `DecompressionStream` is missing.

## 2. `enjin-embed.js` — a GIF, but for videogames

```html
<div class="enjin-embed"
     data-game="/demoroom/playground/"
     data-title="Playground"
     data-poster="/posters/playground.jpg"></div>
<script src="/embed/enjin-embed.js" defer></script>
```

Until someone clicks, an embed costs a poster image and nothing else. No engine,
no wasm, no pak. Click and it boots in place, with fullscreen and close buttons.

Attributes: `data-game` (required, the directory holding `EnjinPlayer.js`),
`data-title`, `data-poster`, `data-hint`, `data-aspect` (default `16/9`),
`data-version` (cache-buster passed through as `?v=`).

`Esc` closes. `window.EnjinEmbed.close()` closes programmatically.

### Why it runs in an iframe

Emscripten hangs its runtime off a single global `Module`, so two players cannot
share a page and there is no clean way to unload one. An iframe gives real
isolation and, more importantly, real teardown: removing it hands the entire heap
plus GPU resources back to the browser. Only one game runs at a time — starting a
second tears the first down — because each live player holds a WebGPU device and
phones fall over well before the second one.

`demo-feed.html` is a working example with several posts.
