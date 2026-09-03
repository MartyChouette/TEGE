"""Generate one self-contained share page per game.

WHY
The script-tag embed only works if you own the page you are posting on. Almost
nowhere you would actually post a game lets you inject JavaScript - Reddit,
Mastodon, Discord, forums, Notion, Substack and most CMSes strip it. What DOES
travel everywhere is a URL, and after that a plain <iframe>. So the shareable
unit has to be a page, not a snippet.

Each generated page:
  * is a complete player - paste the link anywhere and it unfurls to a card with
    the poster, click it and the game runs full-page
  * carries Open Graph + Twitter player meta, so links unfurl with a real
    screenshot instead of a blank favicon
  * works inside someone else's <iframe> with no configuration
  * advertises an oEmbed endpoint, so platforms that support oEmbed (WordPress,
    Ghost, Discourse, Notion) turn a pasted link into a live embed by themselves
  * shows a Copy embed code button with the iframe snippet

  python tools/make_share_pages.py <outDir> [--base https://marty64.net]

Reads games.json next to this script.
"""
import argparse, json, os, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))

PAGE = '''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<title>{title}</title>
<meta name="description" content="{desc}">

<!-- Link unfurling: this is what makes a pasted URL look like something. -->
<meta property="og:type" content="website">
<meta property="og:site_name" content="{site_name}">
<meta property="og:title" content="{title}">
<meta property="og:description" content="{desc}">
<meta property="og:url" content="{share_url}">
<meta property="og:image" content="{poster_url}">
<meta property="og:image:width" content="{pw}">
<meta property="og:image:height" content="{ph}">
<meta property="og:image:alt" content="{title}">

<!-- Player card: platforms that honour it play the game inline. -->
<meta name="twitter:card" content="player">
<meta name="twitter:title" content="{title}">
<meta name="twitter:description" content="{desc}">
<meta name="twitter:image" content="{poster_url}">
<meta name="twitter:player" content="{share_url}">
<meta name="twitter:player:width" content="640">
<meta name="twitter:player:height" content="360">

<link rel="alternate" type="application/json+oembed" href="{oembed_url}" title="{title}">
<link rel="canonical" href="{share_url}">

<style>
  :root {{ color-scheme: dark; }}
  * {{ box-sizing: border-box; }}
  html, body {{ margin:0; height:100%; background:#07090d; color:#e6edf3; overflow:hidden;
                font:14px/1.5 ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif; }}
  #shell {{ position:fixed; inset:0; }}
  canvas {{ width:100%; height:100%; display:block; outline:none; touch-action:none; }}

  /* Poster state: everything before the click. */
  #cover {{ position:absolute; inset:0; cursor:pointer; overflow:hidden; }}
  #cover img {{ position:absolute; inset:0; width:100%; height:100%; object-fit:cover;
                transition:transform .4s ease, filter .4s ease; }}
  #cover:hover img {{ transform:scale(1.035); filter:brightness(1.08); }}
  #veil {{ position:absolute; inset:0; display:flex; align-items:center; justify-content:center;
           background:linear-gradient(180deg,rgba(0,0,0,.10),rgba(0,0,0,.5)); }}
  #play {{ width:74px; height:74px; border-radius:50%; background:rgba(10,14,22,.72);
           border:1px solid rgba(255,255,255,.24); display:flex; align-items:center;
           justify-content:center; transition:transform .2s ease, background .2s ease; }}
  #cover:hover #play {{ transform:scale(1.08); background:rgba(31,111,235,.92); }}
  #cap {{ position:absolute; left:14px; right:14px; bottom:12px; display:flex; gap:9px;
          align-items:baseline; text-shadow:0 1px 4px rgba(0,0,0,.75); pointer-events:none; }}
  #cap b {{ font-size:15px; }}
  #cap span {{ font-size:12px; color:#c2cbd6; }}

  /* Loading + failure */
  #load {{ position:absolute; inset:0; display:none; flex-direction:column; align-items:center;
           justify-content:center; gap:13px; background:#0b0d12; }}
  #load.on {{ display:flex; }}
  #bar {{ width:min(240px,58vw); height:3px; background:#21262d; border-radius:2px; overflow:hidden; }}
  #fill {{ height:100%; width:0; background:#58a6ff; transition:width .18s ease; }}
  #msg {{ font-size:12px; color:#8b949e; font-variant-numeric:tabular-nums; }}
  #oops {{ display:none; max-width:min(440px,84vw); text-align:center; color:#f0883e;
           font-size:13px; line-height:1.6; padding:0 16px; }}
  .mark {{ font-weight:600; letter-spacing:.24em; font-size:11px; color:#8b949e; }}

  /* Tools, once it is running */
  #tools {{ position:absolute; top:10px; right:10px; display:flex; gap:6px; opacity:0;
            transition:opacity .2s ease; z-index:5; }}
  #shell:hover #tools, #shell:focus-within #tools {{ opacity:1; }}
  .tbtn {{ width:32px; height:32px; border-radius:8px; border:1px solid rgba(255,255,255,.2);
           background:rgba(10,14,22,.78); color:#e6edf3; cursor:pointer; display:flex;
           align-items:center; justify-content:center; padding:0; }}
  .tbtn:hover {{ background:rgba(31,111,235,.92); }}

  /* Share sheet */
  #share {{ position:absolute; inset:0; display:none; align-items:center; justify-content:center;
            background:rgba(4,6,10,.82); z-index:10; padding:18px; }}
  #share.on {{ display:flex; }}
  .card {{ background:#0f141b; border:1px solid #21262d; border-radius:12px; padding:16px;
           width:min(520px,100%); }}
  .card h3 {{ margin:0 0 4px; font-size:14px; }}
  .card p {{ margin:0 0 12px; color:#8b949e; font-size:12px; }}
  .row {{ display:flex; gap:7px; margin-bottom:9px; }}
  .row input {{ flex:1; min-width:0; background:#161b22; border:1px solid #30363d; color:#e6edf3;
                border-radius:7px; padding:8px 10px; font:12px ui-monospace,Menlo,Consolas,monospace; }}
  .row button {{ background:#1f6feb; border:0; color:#fff; border-radius:7px; padding:8px 13px;
                 font:inherit; font-size:12px; cursor:pointer; white-space:nowrap; }}
  .row button:hover {{ background:#388bfd; }}
  .close {{ background:none; border:0; color:#8b949e; cursor:pointer; font-size:12px;
            padding:4px 0 0; text-decoration:underline; }}
</style>
</head>
<body>
<div id="shell">
  <canvas id="game" tabindex="0" oncontextmenu="return false"></canvas>

  <div id="cover">
    <img src="{poster_rel}" alt="{title}" decoding="async">
    <div id="veil">
      <div id="play">
        <svg width="26" height="26" viewBox="0 0 24 24" fill="#fff"><path d="M8 5v14l11-7z"/></svg>
      </div>
    </div>
    <div id="cap"><b>{title}</b><span>{hint}</span></div>
  </div>

  <div id="load">
    <div class="mark">{site_name}</div>
    <div id="bar"><div id="fill"></div></div>
    <div id="msg">starting</div>
    <div id="oops"></div>
  </div>

  <div id="tools">
    <button class="tbtn" id="bshare" title="Share / embed">
      <svg width="15" height="15" viewBox="0 0 20 20" fill="currentColor"><path d="M14 7V4l6 6-6 6v-3H9a5 5 0 00-5 5v1H2v-1a7 7 0 017-7h5z"/></svg>
    </button>
    <button class="tbtn" id="bfull" title="Fullscreen">
      <svg width="15" height="15" viewBox="0 0 20 20" fill="currentColor"><path d="M3 3h5v2H5v3H3V3zm9 0h5v5h-2V5h-3V3zM3 12h2v3h3v2H3v-5zm12 3h-3v2h5v-5h-2v3z"/></svg>
    </button>
  </div>

  <div id="share">
    <div class="card">
      <h3>Share this game</h3>
      <p>The link works anywhere. The iframe works on any site that allows one, with no script needed.</p>
      <div class="row">
        <input id="ilink" readonly value="{share_url}">
        <button data-copy="ilink">Copy link</button>
      </div>
      <div class="row">
        <input id="iframe" readonly value='{iframe_snippet}'>
        <button data-copy="iframe">Copy embed</button>
      </div>
      <button class="close" id="bclose">close</button>
    </div>
  </div>
</div>

<script src="{boot_rel}"></script>
<script>
(function () {{
  var GAME = '{game_rel}';
  var VERSION = '{version}';
  var cover = document.getElementById('cover');
  var load  = document.getElementById('load');
  var fill  = document.getElementById('fill');
  var msg   = document.getElementById('msg');
  var oops  = document.getElementById('oops');
  var started = false;

  function fail(t) {{
    load.classList.add('on');
    document.getElementById('bar').style.display = 'none';
    msg.style.display = 'none';
    oops.style.display = 'block';
    oops.textContent = t;
  }}

  function start() {{
    if (started) return;
    started = true;
    cover.style.display = 'none';
    load.classList.add('on');

    if (!navigator.gpu) {{
      fail('This game needs WebGPU. On iPhone or iPad update to iOS 26 and open it in Safari; on a computer use a recent Chrome or Edge, or Safari 26.');
      return;
    }}
    EnjinBoot.boot({{
      baseUrl: GAME,
      canvas: document.getElementById('game'),
      version: VERSION,
      onProgress: function (f, label) {{ fill.style.width = Math.round(f * 100) + '%'; msg.textContent = label; }},
      onReady: function () {{ load.classList.remove('on'); document.getElementById('game').focus(); }}
    }}).catch(function (e) {{ fail('Could not start the game: ' + e.message); }});
  }}

  cover.addEventListener('click', start);
  addEventListener('keydown', function (e) {{
    if (!started && (e.key === 'Enter' || e.key === ' ')) {{ e.preventDefault(); start(); }}
  }});

  // Fullscreen, with the iPhone Safari fallback (it refuses the API for
  // anything but <video>, and inside someone else's iframe it may be blocked
  // outright - in that case the link still opens the game full-page).
  document.getElementById('bfull').addEventListener('click', function () {{
    var el = document.documentElement;
    if (document.fullscreenElement || document.webkitFullscreenElement) {{
      (document.exitFullscreen || document.webkitExitFullscreen || function () {{}}).call(document);
      return;
    }}
    var fn = el.requestFullscreen || el.webkitRequestFullscreen;
    if (fn) {{ try {{ var p = fn.call(el); if (p && p.catch) p.catch(function () {{}}); }} catch (e) {{}} }}
  }});

  var share = document.getElementById('share');
  document.getElementById('bshare').addEventListener('click', function () {{ share.classList.add('on'); }});
  document.getElementById('bclose').addEventListener('click', function () {{ share.classList.remove('on'); }});
  share.addEventListener('click', function (e) {{ if (e.target === share) share.classList.remove('on'); }});
  document.querySelectorAll('[data-copy]').forEach(function (b) {{
    b.addEventListener('click', function () {{
      var i = document.getElementById(b.dataset.copy);
      i.select();
      var ok = false;
      try {{ ok = document.execCommand('copy'); }} catch (e) {{}}
      if (navigator.clipboard) {{ navigator.clipboard.writeText(i.value).then(function () {{}}, function () {{}}); ok = true; }}
      var was = b.textContent;
      b.textContent = ok ? 'Copied' : 'Press Ctrl+C';
      setTimeout(function () {{ b.textContent = was; }}, 1400);
    }});
  }});
}})();
</script>
</body>
</html>
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out_dir')
    ap.add_argument('--base', default='https://marty64.net',
                    help='absolute site root; needed because og:image must be absolute')
    ap.add_argument('--manifest', default=os.path.join(HERE, 'games.json'))
    a = ap.parse_args()

    base = a.base.rstrip('/')
    cfg = json.load(open(a.manifest))
    site_name = cfg.get('siteName', 'TEGE')
    out_root = os.path.abspath(a.out_dir)

    for g in cfg['games']:
        slug = g['slug']
        d = os.path.join(out_root, slug)
        os.makedirs(d, exist_ok=True)

        share_url = f'{base}/play/{slug}/'
        poster_url = f'{base}/embed/posters/{slug}.jpg'
        oembed_url = f'{base}/play/{slug}/oembed.json'
        iframe = (f'&lt;iframe src="{share_url}" width="640" height="360" frameborder="0" '
                  f'allow="fullscreen; autoplay; gamepad" allowfullscreen&gt;&lt;/iframe&gt;')

        html = PAGE.format(
            title=g['title'], desc=g['description'], hint=g.get('hint', 'click to play'),
            site_name=site_name, share_url=share_url, poster_url=poster_url,
            oembed_url=oembed_url, pw=g.get('posterW', 664), ph=g.get('posterH', 373),
            poster_rel=f'../../embed/posters/{slug}.jpg',
            boot_rel='../../embed/enjin-boot.js',
            game_rel=f'../../demoroom/{g["dir"]}/',
            version=cfg.get('version', ''),
            iframe_snippet=iframe.replace('&lt;', '<').replace('&gt;', '>').replace("'", '&apos;'),
        )
        open(os.path.join(d, 'index.html'), 'w', encoding='utf-8').write(html)

        # Static oEmbed. Platforms that support discovery read the <link> in the
        # page and then fetch this; no server code required.
        oembed = {
            'version': '1.0', 'type': 'rich', 'provider_name': site_name,
            'provider_url': base + '/', 'title': g['title'],
            'author_name': cfg.get('author', ''), 'author_url': base + '/',
            'thumbnail_url': poster_url,
            'thumbnail_width': g.get('posterW', 664), 'thumbnail_height': g.get('posterH', 373),
            'width': 640, 'height': 360,
            'html': (f'<iframe src="{share_url}" width="640" height="360" frameborder="0" '
                     f'allow="fullscreen; autoplay; gamepad" allowfullscreen></iframe>')
        }
        json.dump(oembed, open(os.path.join(d, 'oembed.json'), 'w'), indent=2)
        print(f'  {slug:<12} -> play/{slug}/  ({g["title"]})')

    print(f'{len(cfg["games"])} share pages written under {out_root}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
