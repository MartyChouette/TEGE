import http.server
import socketserver

class WasmHandler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.wasm': 'application/wasm',
        '.js': 'application/javascript',
    }

print("Serving at http://localhost:9090")
with socketserver.TCPServer(("", 9090), WasmHandler) as httpd:
    httpd.serve_forever()
