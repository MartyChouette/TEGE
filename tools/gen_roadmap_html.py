import io
import re
import html

# Read the list straight out of ROADMAP.md. A cached extract goes stale the
# first time the roadmap changes, which is the failure mode this repo keeps
# hitting - a document and the thing it describes drifting apart silently.
_all = io.open('docs/ROADMAP.md', encoding='utf-8').read().replace('\r\n', '\n').split('\n')
_a = next(i for i, l in enumerate(_all) if l.startswith('## The list'))
_b = next(i for i, l in enumerate(_all[_a + 1:], _a + 1)
         if l.startswith('## ') and 'The list' not in l)
lines = _all[_a:_b]

STATUS = '''Windows and Linux both build and ship. The Linux CI job builds the engine, runs the
full ctest suite, a lavapipe render smoke under xvfb, and packs and boots an exported
Linux game on every push. Test suite 149/149. Web is a real target: PBR, shadows,
particles, vegetation, weather, inverted-hull outlines, weighted-blended OIT and a
depth-driven post-process pass all run in the browser. Ray tracing displays end to end
on desktop.'''

body = []
section = None
open_ul = False


def close():
    global open_ul
    if open_ul:
        body.append('</ul>')
        open_ul = False


def inline(t):
    t = html.escape(t)
    t = re.sub(r'`([^`]+)`', r'<code>\1</code>', t)
    t = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', t)
    return t


for raw in lines:
    l = raw.rstrip()
    if l.startswith('### '):
        close()
        body.append('<h2>%s</h2>' % inline(l[4:]))
    elif l.startswith('## '):
        continue
    elif l.startswith('- ['):
        done = l[3] == 'x'
        text = l[6:].strip()
        m = re.match(r'^(\d+)\.\s*(.*)$', text)
        num, rest = (m.group(1), m.group(2)) if m else ('', text)
        if not open_ul:
            body.append('<ul class="items">')
            open_ul = True
        body.append(
            '<li class="%s"><span class="n">%s</span><span class="t">%s</span></li>'
            % ('done' if done else '', num, inline(rest)))
    elif l.strip():
        close()
        body.append('<p class="note">%s</p>' % inline(l.strip()))

close()
BODY = '\n'.join(body)

total = sum(1 for l in lines if l.startswith('- ['))
done_n = sum(1 for l in lines if l.startswith('- [x]'))

DOC = '''<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TEGE &mdash; The 1.0 List</title>
<style>
:root{
  --ground:#F2F4F7;--surface:#FBFCFD;--surface-2:#E8ECF1;--rule:#C9D2DC;--rule-soft:#DDE4EB;
  --ink:#161C24;--ink-2:#3F4A57;--ink-3:#6B7787;
  --ok:#2E6F4E;--ok-bg:#DCEBE2;--accent:#1F4E79;
}
@media (prefers-color-scheme:dark){:root{
  --ground:#0E1319;--surface:#141B23;--surface-2:#1B242E;--rule:#2B3742;--rule-soft:#212B35;
  --ink:#E6ECF2;--ink-2:#B3BECB;--ink-3:#7C8899;
  --ok:#7FCBA1;--ok-bg:#17301F;--accent:#7FB2E0;
}}
*{box-sizing:border-box}
body{margin:0;background:var(--ground);color:var(--ink);
  font:16px/1.55 ui-serif,Georgia,"Times New Roman",serif;-webkit-font-smoothing:antialiased}
.wrap{max-width:60rem;margin:0 auto;padding:2.75rem 1.4rem 5rem}
.eyebrow{font:500 .68rem/1 ui-monospace,SFMono-Regular,Menlo,monospace;letter-spacing:.14em;
  text-transform:uppercase;color:var(--ink-3);margin:0 0 .7rem}
h1{font:700 clamp(1.9rem,5vw,2.7rem)/1.02 ui-sans-serif,"Segoe UI",Helvetica,Arial,sans-serif;
  letter-spacing:-.025em;margin:0 0 .6rem}
.status{max-width:62ch;color:var(--ink-2);margin:0 0 1.4rem}
.bar{display:flex;flex-wrap:wrap;gap:0;border:1px solid var(--rule);background:var(--surface);
  margin-bottom:2.2rem}
.bar div{padding:.8rem 1rem;border-right:1px solid var(--rule-soft);flex:1 1 8rem}
.bar div:last-child{border-right:0}
.bar .k{font:500 .62rem/1 ui-monospace,monospace;letter-spacing:.1em;text-transform:uppercase;
  color:var(--ink-3)}
.bar .v{font:600 1.05rem/1.2 ui-sans-serif,"Segoe UI",Helvetica,Arial,sans-serif;margin-top:.2rem;
  font-variant-numeric:tabular-nums}
h2{font:600 .82rem/1 ui-sans-serif,"Segoe UI",Helvetica,Arial,sans-serif;letter-spacing:.1em;
  text-transform:uppercase;color:var(--ink-3);margin:2.2rem 0 .3rem;
  padding-bottom:.5rem;border-bottom:2px solid var(--ink)}
h2:first-of-type{margin-top:0}
p.note{max-width:62ch;color:var(--ink-2);font-size:.93rem;margin:.8rem 0 0}
ul.items{list-style:none;padding:0;margin:0}
ul.items li{display:grid;grid-template-columns:2.5rem 1fr;gap:.55rem;padding:.5rem 0;
  border-bottom:1px solid var(--rule-soft);align-items:baseline}
ul.items li .n{font:400 .76rem/1.5 ui-monospace,monospace;color:var(--ink-3);
  font-variant-numeric:tabular-nums}
ul.items li .t{min-width:0}
li.done .t{color:var(--ink-3);text-decoration:line-through;text-decoration-color:var(--ink-3)}
li.done .n::after{content:"\\2713";color:var(--ok);margin-left:.3rem}
code{font:.85em/1 ui-monospace,SFMono-Regular,Menlo,monospace;background:var(--surface-2);
  padding:.1em .32em;border-radius:2px}
footer{margin-top:3rem;padding-top:1rem;border-top:1px solid var(--rule);color:var(--ink-3);
  font-size:.84rem;max-width:62ch}
@media print{body{background:#fff;color:#000}.bar{break-inside:avoid}}
</style></head><body>
<div class="wrap">
  <p class="eyebrow">Enjin / TEGE</p>
  <h1>The 1.0 List</h1>
  <p class="status">%s</p>
  <div class="bar">
    <div><div class="k">Ship</div><div class="v">October 2026</div></div>
    <div><div class="k">Price</div><div class="v">$30</div></div>
    <div><div class="k">Source</div><div class="v">BSL 1.1</div></div>
    <div><div class="k">Items</div><div class="v">%d</div></div>
    <div><div class="k">Done</div><div class="v">%d</div></div>
    <div><div class="k">Suite</div><div class="v">149/149</div></div>
  </div>
%s
  <footer>Generated from docs/ROADMAP.md. Source of truth is the markdown; regenerate this
  file rather than editing it.</footer>
</div></body></html>
''' % (html.escape(STATUS), total, done_n, BODY)

io.open('docs/roadmap.html', 'w', encoding='utf-8').write(DOC)
print('docs/roadmap.html written -', total, 'items,', done_n, 'done')
