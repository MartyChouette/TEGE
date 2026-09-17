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
import io
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

        def end_headers(self):
            # CROSS-ORIGIN ISOLATION, which this server did not send.
            #
            # The engine is built with pthreads, so its memory is a
            # SharedArrayBuffer, and a browser only hands one out on a
            # cross-origin-isolated page. Without these two headers the wasm
            # never instantiates -- so every game reported ENGINE NEVER RAN
            # while booting fine under web-demo/serve.py, which does send them.
            #
            # A checker that serves the build differently from the way it is
            # actually served is not checking the deployment. These match
            # serve.py deliberately; if that file's headers change, this one
            # has to follow.
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
            self.send_header("Accept-Ranges", "bytes")
            super().end_headers()

        def send_head(self):
            # RANGE REQUESTS. SimpleHTTPRequestHandler has none, and the engine
            # wasm is 13MB: Chrome asks for a range, gets a full-body 200,
            # gives up on the connection mid-transfer, and streaming
            # instantiation fails. The server then dies on the aborted socket.
            # Net effect was ENGINE NEVER RAN on a build that boots fine.
            rng = self.headers.get("Range")
            if not rng or not rng.startswith("bytes="):
                return super().send_head()
            path = self.translate_path(self.path)
            if not os.path.isfile(path):
                return super().send_head()
            size = os.path.getsize(path)
            spec = rng[6:].split("-", 1)
            try:
                start = int(spec[0]) if spec[0] else 0
                end = int(spec[1]) if len(spec) > 1 and spec[1] else size - 1
            except ValueError:
                return super().send_head()
            end = min(end, size - 1)
            if start > end:
                self.send_response(416)
                self.send_header("Content-Range", "bytes */%d" % size)
                self.end_headers()
                return None
            fh = open(path, "rb")
            fh.seek(start)
            self.send_response(206)
            self.send_header("Content-Type", self.guess_type(path))
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, size))
            self.send_header("Content-Length", str(end - start + 1))
            self.end_headers()
            # SimpleHTTPRequestHandler copies to EOF, so hand it only the slice.
            data = fh.read(end - start + 1)
            fh.close()
            return io.BytesIO(data)

        def handle_one_request(self):
            # A browser that has what it needs closes the socket, and that is
            # not a server error. Letting it surface printed a traceback
            # through the middle of the report.
            try:
                super().handle_one_request()
            except (ConnectionAbortedError, ConnectionResetError, BrokenPipeError):
                self.close_connection = True

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
    refs = {r for r in refs if not r.startswith(("http", "//", "#"))}

    # WHERE THE ENGINE ACTUALLY LIVES. This used to be hardcoded to
    # "EnjinPlayer.wasm" beside the page, which was true when every demo carried
    # its own copy of the engine. It is not true now: the sub-demos load
    # '../EnjinPlayer.js' and share one engine at the demo root.
    #
    # The old code got it wrong twice over. It DROPPED any '..' reference as
    # out-of-directory, so it never checked the real engine script at all, and
    # then it added a same-directory wasm that had never existed -- so all five
    # sub-demos reported a 404 on a file nothing requests, while booting
    # perfectly in a browser. Emscripten resolves the wasm against the SCRIPT's
    # URL, not the document's, so the engine script's own src is the only honest
    # place to get this from.
    script = re.findall(r"\.src\s*=\s*['\"]([^'\"]*EnjinPlayer\.js[^'\"]*)['\"]", html)
    engine_js = script[0] if script else "EnjinPlayer.js"
    refs.add(engine_js)

    # The '?v=' cache-buster is optional: a locateFile hook versions the wasm
    # separately from the script tag, and bumping only one ships a mismatched
    # pair. No demo uses one today, so the plain path is what a browser asks for.
    ver = re.findall(r"path\s*\+\s*'\?v=([0-9A-Za-z]+)'", html)
    wasm = engine_js[:-3] + ".wasm" if engine_js.endswith(".js") else "EnjinPlayer.wasm"
    refs.add(wasm + ("?v=" + ver[0] if ver else ""))
    if os.path.isfile(os.path.join(base_dir, game, "game.enjpak")):
        refs.add("game.enjpak")

    problems, wasm_size = [], 0
    for r in sorted(refs):
        st, ln = http_status("%s%s/%s" % (base, game, r))
        if st != 200:
            problems.append("%s -> HTTP %s" % (r, st))
        if r.endswith(".wasm") or ".wasm?" in r:
            wasm_size = ln
    return problems, wasm_size


def check_boot(chrome, base, game, attempts=6):
    """Did the engine actually run? Retries only a harness race, never a failure.

    Headless Chrome sometimes exits before the page has logged ANYTHING -- the
    run takes ~0.7s instead of the ~1.5s a real boot takes, and the stderr holds
    no CONSOLE line at all. That is the browser losing a startup race, and it is
    indistinguishable in the result from a build that cannot boot, which is how
    a healthy demo room reported ENGINE NEVER RAN about one run in three.

    The two cases separate cleanly on whether the PAGE ever spoke:

      no CONSOLE lines at all      the harness saw nothing -- retry
      CONSOLE lines, no engine     the page ran and the engine did not -- FAIL

    So only the first is retried. A build that genuinely fails to instantiate
    still logs its way to the failure and is reported on the first attempt.

    THREE ATTEMPTS WAS NOT ENOUGH, measured 2026-09-16: on a machine that had
    just finished an Emscripten build, this reported FAIL on three consecutive
    runs with six games, a different game each time, while every one of those
    games rendered perfectly under tools/web_capture.mjs. The losing runs had
    stderr of 312 to 578 bytes -- Chrome's own startup noise and not one CONSOLE
    line -- which is precisely the race described above, three times running.
    Six costs nothing when the first attempt succeeds, which is almost always.

    Set ENJIN_DEMOROOM_DEBUG=1 to print the browser's stderr for a failing
    attempt. Without it a red result says "the engine never ran" and nothing
    about why, and two plausible fixes were guessed at and reverted before
    anybody read the log.
    """
    for attempt in range(attempts):
        ran, fetch_fail, spoke = _boot_once(chrome, base, game)
        if ran or spoke:
            return ran, fetch_fail
    return False, fetch_fail


def _boot_once(chrome, base, game):
    with tempfile.TemporaryDirectory() as tmp:
        err = os.path.join(tmp, "err.txt")
        with open(err, "wb") as fh:
            subprocess.run(
                # WebGPU IN HEADLESS. --disable-gpu was here, and it is the
                # one flag that guarantees this check can never pass: the demo
                # pages gate the engine behind navigator.gpu.requestAdapter(),
                # which returns null without an adapter, so the page shows its
                # "no WebGPU" card and the engine is never fetched. Every game
                # then reports ENGINE NEVER RAN whatever it actually does in a
                # browser -- a red result that says nothing about the build.
                # These are the flags tools/web_capture.mjs uses to get a real
                # software adapter out of headless Chrome.
                [chrome, "--headless=new", "--no-sandbox",
                 "--enable-unsafe-webgpu", "--enable-unsafe-swiftshader",
                 "--enable-features=Vulkan",
                 "--virtual-time-budget=25000", "--enable-logging=stderr", "--v=0",
                 "--user-data-dir=" + os.path.join(tmp, "profile"),
                 "--dump-dom", "%s%s/index.html" % (base, game)],
                stdout=subprocess.DEVNULL, stderr=fh, timeout=180)
        log = open(err, encoding="utf-8", errors="replace").read()
    ran = any(m in log for m in ENGINE_RAN)
    if not ran and os.environ.get("ENJIN_DEMOROOM_DEBUG"):
        # A red result here says "the engine never ran" and nothing about WHY.
        # Set ENJIN_DEMOROOM_DEBUG=1 to get the browser's own account of it,
        # which is the difference between diagnosing this and guessing at it
        # twice, as happened on 2026-09-16.
        sys.stderr.write("\n===== FAILING LOG: %s (%d bytes) =====\n" % (game, len(log)))
        sys.stderr.write(log[-4000:])
        sys.stderr.write("\n===== end =====\n")
    fetch_fail = re.findall(r"Failed to load resource.*?(\S+)", log)
    # Did the PAGE produce any console output at all? Chrome's own startup
    # warnings are always present, so their absence proves nothing -- only a
    # CONSOLE line means the document got as far as running script.
    spoke = "INFO:CONSOLE" in log or "CONSOLE(" in log
    return ran, fetch_fail, spoke


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
    # localhost, NOT 127.0.0.1. Headless Chrome refuses a WebGPU adapter on the
    # raw-IP origin here, so the demo pages' requestAdapter() gate fails, the
    # engine script is never appended, and every game reports ENGINE NEVER RAN.
    # Measured both ways against both servers: localhost boots, 127.0.0.1 does
    # not. The bind address stays 127.0.0.1 -- only the URL the browser is given
    # has to be the hostname.
    base = "http://localhost:%d/" % PORT
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
