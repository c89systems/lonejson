"""Serve the existing rewind fixture on a loopback port for native HTTP tests."""
from http.server import HTTPServer
from pathlib import Path
import signal
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "devenv" / "sink"))
from app import Handler

signal.signal(signal.SIGTERM, lambda *_args: sys.exit(0))
server = HTTPServer(("127.0.0.1", 0), Handler)
Path(sys.argv[1]).write_text(str(server.server_port))
server.serve_forever()
