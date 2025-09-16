import http.server
import socketserver

PORT = 8080

class TestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        http.server.SimpleHTTPRequestHandler.end_headers(self)

socketserver.TCPServer.allow_reuse_address = True        
with socketserver.TCPServer(("", PORT), TestHandler) as httpd:
    print(f"serving at port {PORT}")
    httpd.serve_forever();
