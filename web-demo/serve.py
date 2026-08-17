#!/usr/bin/env python3
"""Local dev server for the Enjin web player (WASM + WebGPU).

Serves build-web/bin/EnjinPlayer.{js,wasm,data} at http://localhost:9090 with the
MIME types, no-cache, HTTP range support, and (optionally) the cross-origin
isolation headers a WASM build needs. The latest build artifacts are auto-copied
from build-web/bin/ on start.

  python serve.py                 # copy latest build, serve, open the browser
  python serve.py --no-open       # do not open the browser
  python serve.py --build         # configure (if needed) + build the player first, then serve
  python serve.py --port 8080     # change port (default 9090)
  python serve.py --coop          # send COOP/COEP (required if the build uses WASM threads)
  python serve.py --tunnel        # expose over HTTPS via cloudflared/ngrok for remote testers

Notes:
- The current web build is single-threaded WebGPU, so it does NOT need --coop.
  The moment you enable Emscripten pthreads (SharedArrayBuffer), --coop becomes
  mandatory or the module will not instantiate.
- --build prefers emmake/emcmake on PATH (emsdk activated); otherwise it uses the
  EMSDK + EMSDK_PYTHON environment variables the project docs build with.
"""
import argparse
import http.server
import os
import shutil
import subprocess
import sys
import threading
import webbrowser

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD_DIR = os.path.join(ROOT, 'build-web')
BIN_DIR = os.path.join(BUILD_DIR, 'bin')
# .data only exists when a project is packed with --preload-file; copied if present.
ARTIFACTS = ['EnjinPlayer.js', 'EnjinPlayer.wasm', 'EnjinPlayer.data']


def copy_artifacts():
    """Copy any newer build artifacts from build-web/bin/ into this folder."""
    for fname in ARTIFACTS:
        src = os.path.join(BIN_DIR, fname)
        dst = os.path.join(HERE, fname)
        if os.path.exists(src):
            if not os.path.exists(dst) or os.path.getmtime(src) > os.path.getmtime(dst):
                shutil.copy2(src, dst)
                print(f"Updated {fname} from build-web/bin/")


def em_prefix(tool):
    """argv prefix to run an emscripten tool (emmake/emcmake).

    Prefers the tool on PATH (emsdk activated); falls back to EMSDK + EMSDK_PYTHON.
    Returns None if neither is available.
    """
    if shutil.which(tool):
        return [tool]
    emsdk = os.environ.get('EMSDK')
    py = os.environ.get('EMSDK_PYTHON')
    if emsdk and py:
        return [py, os.path.join(emsdk, 'upstream', 'emscripten', tool + '.py')]
    return None


def build_player():
    """Configure build-web (if needed) and build the EnjinPlayer web target."""
    emmake = em_prefix('emmake')
    if not emmake:
        sys.exit("Cannot build: activate emsdk (so emmake is on PATH) or set EMSDK + EMSDK_PYTHON.")

    if not os.path.exists(os.path.join(BUILD_DIR, 'CMakeCache.txt')):
        emcmake = em_prefix('emcmake')
        if not emcmake:
            sys.exit("Cannot configure: activate emsdk (emcmake on PATH) or set EMSDK + EMSDK_PYTHON.")
        cfg = emcmake + ['cmake', '-B', BUILD_DIR, '-S', ROOT, '-DENJIN_PLATFORM_WEB=ON']
        print("Configuring:", ' '.join(cfg))
        if subprocess.call(cfg) != 0:
            sys.exit("Configure failed.")

    cmd = emmake + ['cmake', '--build', BUILD_DIR, '--target', 'EnjinPlayer']
    print("Building:", ' '.join(cmd))
    if subprocess.call(cmd) != 0:
        sys.exit("Build failed.")


def start_tunnel(port):
    """Expose the local server over HTTPS for remote testers. Tries cloudflared, then ngrok."""
    if shutil.which('cloudflared'):
        print("Starting cloudflared tunnel (watch for the https://...trycloudflare.com URL below)...")
        return subprocess.Popen(['cloudflared', 'tunnel', '--url', f'http://localhost:{port}'])
    if shutil.which('ngrok'):
        print("Starting ngrok tunnel (open http://localhost:4040 for the public URL)...")
        return subprocess.Popen(['ngrok', 'http', str(port)])
    print("No tunnel tool found. Install cloudflared or ngrok for remote playtest.")
    return None


class PlayerHandler(http.server.SimpleHTTPRequestHandler):
    """WASM MIME types, no-cache, HTTP range (206), and optional COOP/COEP."""
    coop = False  # set from --coop

    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.wasm': 'application/wasm',
        '.js': 'application/javascript',
        '.data': 'application/octet-stream',
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=HERE, **kwargs)

    def end_headers(self):
        # Fresh builds every load during dev.
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        self.send_header('Accept-Ranges', 'bytes')
        # Cross-origin isolation. Required once the build enables WASM threads
        # (SharedArrayBuffer). The current single-threaded WebGPU build does not
        # need it, so it is opt-in via --coop.
        if self.coop:
            self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
            self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

    def do_GET(self):
        # Serve HTTP range requests (206) for streamed media/assets; fall back to
        # the normal full-file path for everything else.
        rng = self.headers.get('Range')
        path = self.translate_path(self.path)
        if rng is None or not os.path.isfile(path):
            return super().do_GET()
        size = os.path.getsize(path)
        try:
            start, end = self._parse_range(rng, size)
        except ValueError:
            self.send_response(416)
            self.send_header('Content-Range', f'bytes */{size}')
            self.end_headers()
            return
        length = end - start + 1
        self.send_response(206)
        self.send_header('Content-Type', self.guess_type(path))
        self.send_header('Content-Range', f'bytes {start}-{end}/{size}')
        self.send_header('Content-Length', str(length))
        self.end_headers()
        with open(path, 'rb') as f:
            f.seek(start)
            remaining = length
            while remaining > 0:
                chunk = f.read(min(64 * 1024, remaining))
                if not chunk:
                    break
                self.wfile.write(chunk)
                remaining -= len(chunk)

    @staticmethod
    def _parse_range(header, size):
        """Parse a single 'bytes=start-end' (or suffix 'bytes=-N') range."""
        units, _, spec = header.partition('=')
        if units.strip().lower() != 'bytes':
            raise ValueError('unsupported range unit')
        start_s, _, end_s = spec.strip().partition('-')
        if start_s == '':
            n = int(end_s)
            if n <= 0:
                raise ValueError('bad suffix range')
            start, end = max(0, size - n), size - 1
        else:
            start = int(start_s)
            end = int(end_s) if end_s else size - 1
        end = min(end, size - 1)
        if start > end or start >= size:
            raise ValueError('range out of bounds')
        return start, end


def main():
    ap = argparse.ArgumentParser(description="Serve the Enjin web player locally.")
    ap.add_argument('--port', type=int, default=9090)
    ap.add_argument('--build', action='store_true', help="Configure (if needed) + build the player first.")
    ap.add_argument('--no-open', action='store_true', help="Do not open the browser.")
    ap.add_argument('--coop', action='store_true', help="Send COOP/COEP (WASM threads / SharedArrayBuffer).")
    ap.add_argument('--tunnel', action='store_true', help="Expose over HTTPS via cloudflared/ngrok.")
    args = ap.parse_args()

    if args.build:
        build_player()
    copy_artifacts()

    PlayerHandler.coop = args.coop
    tunnel_proc = start_tunnel(args.port) if args.tunnel else None

    url = f'http://localhost:{args.port}'
    if not args.no_open:
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()

    print(f"Serving {HERE} at {url}")
    print(f"COOP/COEP: {'on (cross-origin isolated)' if args.coop else 'off (single-threaded build)'}")
    try:
        with http.server.ThreadingHTTPServer(("", args.port), PlayerHandler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping.")
    finally:
        if tunnel_proc:
            tunnel_proc.terminate()


if __name__ == '__main__':
    main()
