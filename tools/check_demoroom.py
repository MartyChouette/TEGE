#!/usr/bin/env python3
"""Verify a deployed demo room before it goes live.

Two things have shipped broken to the site before, and neither is visible by
looking at the files:

  1. A stale ENGINE. Each game's index.html versions the wasm through its own
     locateFile hook, SEPARATE from the <script> tag's cache-buster. Bump only
     the script tag and a browser fetches the new EnjinPlayer.js against the
     cached old EnjinPlayer.wasm -- a mismatched pair, and a black canvas.

  2. A wasm that does not instantiate at all. The files are present and the
     right size, and the page still shows nothing.

So this serves the folder over real HTTP, fetches every asset a browser would
(query strings included), and boots each game in headless Chrome to confirm the
engine binary actually starts.

Headless Chrome on a machine with no GPU has NO WebGPU adapter, so a successful
boot ends at "Failed to get adapter". That is the pass condition: reaching it
proves the wasm loaded, the logger initialised and the player ran. It cannot
tell you the game renders correctly -- only a real browser can.

    python tools/check_demoroom.py <demoroom-dir>

Exits non-zero if any asset 404s, if the games disagree about which engine they
ship, or if a game's wasm never instantiates.
"""
import http.server
import os
import re
import subprocess
import sys
import tempfile
import threading
import urllib.request

PORT = 8973

CHROME_CANDIDATES = [
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    "/usr/bin/google-chrome",
    "/usr/bin/chromium",
]

# Log lines only the engine can produce. Seeing any of them means the wasm
# instantiated and ran, whatever happened afterwards.
ENGINE_RAN = (
    "web_main.cpp",
    "WebGPURenderer.cpp",
    "[PLAYER]",
    "[CORE  ]",
)


def find_chrome():
    for c in CHROME_CANDIDATES:
        if os.path.isfile(c):
            return c
    return None


def serve(root):
    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *a):   # the per-request log buries the report
            pass

    handler = lambda *a, **k: Quiet(*a, directory=root, **k)
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", PORT), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return httpd


def http_status(url):
    try:
        r = urllib.request.urlopen(url, timeout=30)
        return r.status, int(r.headers.get("Content-Length") or 0)
    except Exception as e:
        return getattr(e, "code", 0), 0


def game_dirs(root):
    out = []
    for name in sorted(os.listdir(root)):
        if os.path.isfile(os.path.join(root, name, "index.html")):
            out.append(name)
    return out


def check_assets(base, game):
    """Every asset the page references, plus the versioned wasm and the pak."""
    idx = "%s%s/index.html" % (base, game)
    st, _ = http_status(idx)
    if st != 200:
        return ["index.html HTTP %s" % st], 0

    html = urllib.request.urlopen(idx, timeout=30).read().decode("utf-8", "replace")
    refs = set(re.findall(r'(?:src|href)="([^"]+)"', html))
    refs = {r for r in refs if not r.startswith(("http", "//", "#", ".."))}

    # The locateFile hook is what versions the WASM request. Checking the plain
    # filename would miss exactly the mismatch this script exists to catch.
    ver = re.findall(r"path\s*\+\s*'\?v=([0-9A-Za-z]+)'", html)
    refs.add("EnjinPlayer.wasm" + ("?v=" + ver[0] if ver else ""))
    if os.path.isfile(os.path.join(base_dir, game, "game.enjpak")):
        refs.add("game.enjpak")

    problems, wasm_size = [], 0
    for r in sorted(refs):
        st, ln = http_status("%s%s/%s" % (base, game, r))
        if st != 200:
            problems.append("%s -> HTTP %s" % (r, st))
        if r.startswith("EnjinPlayer.wasm"):
            wasm_size = ln
    return problems, wasm_size


def check_boot(chrome, base, game):
    with tempfile.TemporaryDirectory() as tmp:
        err = os.path.join(tmp, "err.txt")
        with open(err, "wb") as fh:
            subprocess.run(
                [chrome, "--headless=new", "--disable-gpu", "--no-sandbox",
                 "--virtual-time-budget=25000", "--enable-logging=stderr", "--v=0",
                 "--user-data-dir=" + os.path.join(tmp, "profile"),
                 "--dump-dom", "%s%s/index.html" % (base, game)],
                stdout=subprocess.DEVNULL, stderr=fh, timeout=180)
        log = open(err, encoding="utf-8", errors="replace").read()
    ran = any(m in log for m in ENGINE_RAN)
    fetch_fail = re.findall(r"Failed to load resource.*?(\S+)", log)
    return ran, fetch_fail


def main():
    global base_dir
    base_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    if not os.path.isfile(os.path.join(base_dir, "index.html")):
        print("no index.html in %s" % base_dir)
        return 2

    games = game_dirs(base_dir)
    if not games:
        print("no games found in %s" % base_dir)
        return 2

    serve(base_dir)
    base = "http://127.0.0.1:%d/" % PORT
    chrome = find_chrome()
    if not chrome:
        print("note: no Chrome found, skipping the boot check (assets still verified)")

    failures = 0
    wasm_sizes = {}
    print("%-16s %-8s %-12s %s" % ("GAME", "ASSETS", "WASM", "BOOT"))
    for g in games:
        problems, wasm = check_assets(base, g)
        wasm_sizes[g] = wasm
        boot = "skipped"
        if chrome and not problems:
            ran, fetch_fail = check_boot(chrome, base, g)
            boot = "ok" if ran else "ENGINE NEVER RAN"
            if not ran:
                failures += 1
            if fetch_fail:
                boot += " (%d fetch failures)" % len(fetch_fail)
                failures += 1
        if problems:
            failures += 1
        print("%-16s %-8s %-12s %s" % (
            g, "FAIL" if problems else "ok", wasm or "?", boot))
        for p in problems:
            print("      %s" % p)

    # Every game must ship the SAME engine. A single stale one is how a demo
    # ends up on an old build while the rest are current.
    distinct = {s for s in wasm_sizes.values() if s}
    if len(distinct) > 1:
        print("\nMIXED ENGINES: %s" % ", ".join(
            "%s=%d" % (g, s) for g, s in sorted(wasm_sizes.items())))
        failures += 1

    print("\n%s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
