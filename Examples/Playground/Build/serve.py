import http.server, socketserver
class H(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      '.wasm': 'application/wasm', '.js': 'application/javascript'}
PORT = 9090
print(f'Serving at http://localhost:{PORT}')
with socketserver.TCPServer(('', PORT), H) as httpd:
    httpd.serve_forever()
