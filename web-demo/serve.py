import http.server
import socketserver
import shutil
import os

# Auto-copy latest build from build-web/bin/ to web-demo/
BUILD_DIR = os.path.join(os.path.dirname(__file__), '..', 'build-web', 'bin')
SERVE_DIR = os.path.dirname(__file__)

for fname in ['EnjinPlayer.js', 'EnjinPlayer.wasm']:
    src = os.path.join(BUILD_DIR, fname)
    dst = os.path.join(SERVE_DIR, fname)
    if os.path.exists(src):
        src_mtime = os.path.getmtime(src)
        dst_mtime = os.path.getmtime(dst) if os.path.exists(dst) else 0
        if src_mtime > dst_mtime:
            shutil.copy2(src, dst)
            print(f"Updated {fname} from build-web/bin/")

class WasmHandler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.wasm': 'application/wasm',
        '.js': 'application/javascript',
    }
    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()

print("Serving at http://localhost:9090")
with socketserver.TCPServer(("", 9090), WasmHandler) as httpd:
    httpd.serve_forever()
