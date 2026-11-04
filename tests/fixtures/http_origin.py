"""Small loopback origin serving declared cache-test bytes and a request log."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import signal
import sys

payload, log, port = map(Path, sys.argv[1:])


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        with log.open("a") as output:
            output.write(self.path + "\n")
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"corrupt" if self.path.startswith("/bad") else payload.read_bytes())

    def do_HEAD(self):
        with log.open("a") as output:
            output.write("HEAD " + self.path + "\n")
        self.send_response(200)
        self.end_headers()

    def log_message(self, *_args):
        pass


signal.signal(signal.SIGTERM, lambda *_args: sys.exit(0))
server = HTTPServer(("127.0.0.1", 0), Handler)
port.write_text(str(server.server_port))
server.serve_forever()
