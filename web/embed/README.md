# Posting an Enjin game somewhere on the internet

## The actual constraint

Almost nowhere you would post a game lets you inject JavaScript. Reddit,
Mastodon, Discord, forums, Substack and most CMSes strip `<script>` on sight. So
a script-tag embed, however nice, only works on pages you already own.

What travels everywhere is **a URL**. After that, **a plain `<iframe>`**. Those
are the two units to build for; everything else is a bonus.

## 1. A URL per game — the primary unit

`tools/make_share_pages.py` generates `/play/<slug>/` for each game. Each page:

- is a complete player: paste the link anywhere, click, the game runs
- carries Open Graph and Twitter player meta, so the link unfurls with the real
  screenshot instead of a blank favicon
- works inside somebody else's `<iframe>` with no configuration
- ships a static `oembed.json` and advertises it with `<link rel="alternate">`,
  so platforms that support oEmbed discovery turn a pasted link into a live
  embed by themselves
- has a share button that copies the link or the iframe snippet

```bash
python tools/make_share_pages.py <siteRoot>/play --base https://marty64.net
```

Games are listed in `tools/games.json`.

### What that gets you where

| Where | Result |
|---|---|
| Any site allowing iframes (Ghost, WordPress, Notion, Confluence, itch devlogs) | plays inline |
| Discord, Slack, Mastodon, Reddit, iMessage | rich card with the poster; click opens the game |
| Platforms with oEmbed discovery | pasted link becomes a live embed |
| X / Twitter | player card tags are present, but inline play needs their whitelist |

The honest limit: Reddit, Mastodon and Discord do not allow third-party iframes,
so there the link is a card that opens the game in a tab. No embed technique
changes that; it is their policy, not a technical gap.

## 2. `enjin-embed.js` — for pages you own

```html
<div class="enjin-embed"
     data-game="/demoroom/playground/"
     data-poster="/embed/posters/playground.jpg"
     data-title="Playground"></div>
<script src="/embed/enjin-embed.js" defer></script>
```

Until someone clicks, an embed costs a poster and nothing else, so a page can
carry many. Attributes: `data-game` (required), `data-title`, `data-poster`,
`data-hint`, `data-aspect` (default `16/9`), `data-version`. `Esc` closes.

Each game runs in an iframe on purpose: Emscripten's runtime is a page-global
singleton with no clean unload, so an iframe is what makes teardown real and
many-per-page safe. One game runs at a time.

## 3. `enjin-boot.js` — stop shipping 11.6 MB

`EnjinPlayer.wasm` is 11.56 MB raw and 3.22 MB gzipped, but a host that does not
set `Content-Encoding` on `application/wasm` sends the whole thing:

```
$ curl -sI https://marty64.net/demoroom/playground/EnjinPlayer.wasm
HTTP/1.1 200 OK
Content-Length: 12121999      <- no content-encoding at all
Server: Apache
```

`enjin-boot.js` fetches `EnjinPlayer.wasm.gz`, inflates it with the browser's
native `DecompressionStream`, and hands the bytes to Emscripten via
`Module.wasmBinary`. Measured: **12,121,999 bytes → 3,372,538**, with the plain
`.wasm` never requested. It falls back to the uncompressed path whenever the
`.gz` or `DecompressionStream` is missing.

Regenerate on every deploy, or you ship a stale engine:

```bash
gzip -9 -c EnjinPlayer.wasm > EnjinPlayer.wasm.gz
```

`enjin.htaccess` is the server-side version of the same fix, and covers every
asset rather than just the engine. Review before installing.

## The remaining portability blocker

WebGPU. No WebGPU, no game — that means iOS 18+ with Safari, or a recent Chrome,
Edge or Safari on desktop. Everyone else gets an explanatory card. That is a far
bigger reach limit than any embed detail, and closing it means a WebGL2 fallback
renderer, which is a large piece of work rather than a tweak.

## Posters

`tools/capture_poster.py` renders a still through the editor's `--golden` path on
the real GPU. It copies the project to a scratch directory, strips the character
controllers (otherwise they drive the camera and you capture wherever the player
was standing), poses the camera, and captures. The real project is untouched.

```bash
python tools/capture_poster.py <project.enjinproject> <out_base> \
    --pos 33 6 18 --look 16 2 -10 --fov 68
python tools/capture_poster.py <project.enjinproject> <out_base> --scene-camera
```
