#!/usr/bin/env python3
"""Loopback-only deterministic test station, never contacted by the device app."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys
root = Path(sys.argv[1]); port_file = Path(sys.argv[2])
class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        ext = self.path.strip('/').split('?')[0]
        if ext == 'redirect':
            self.send_response(302); self.send_header('Location', '/mp3')
            self.send_header('Content-Type', 'text/html'); self.end_headers()
            self.wfile.write(b'<html>redirect</html>'); return
        if ext == 'html':
            self.send_response(200); self.send_header('Content-Type', 'text/html')
            self.end_headers(); self.wfile.write(b'<html>not a stream</html>'); return
        if ext == 'rawicy':
            self.wfile.write(b'ICY 200 OK\r\nContent-Type: audio/mpeg\r\n\r\n')
            try: self.wfile.write((root/'test.mp3').read_bytes()*2)
            except (BrokenPipeError, ConnectionResetError): pass
            return
        if ext not in ('mp3', 'opus', 'ogg', 'icy', 'oggchain', 'opuschain'):
            self.send_error(404); return
        source = root / ('test.mp3' if ext == 'icy' else ext + '.ogg' if ext == 'oggchain' else ext + '.opus' if ext == 'opuschain' else 'test.' + ext)
        self.send_response(200)
        self.send_header('Content-Type', {'mp3':'audio/mpeg', 'icy':'audio/mpeg', 'opus':'audio/ogg', 'ogg':'audio/ogg', 'oggchain':'audio/ogg', 'opuschain':'audio/ogg'}[ext])
        if ext == 'icy': self.send_header('icy-metaint', '8192')
        self.end_headers()
        audio = source.read_bytes()
        try:
            for _ in range(1 if ext.endswith('chain') else 2):
                if ext == 'icy':
                    metadata = b"StreamTitle='RedAmp Test Track';".ljust(48, b'\0')
                    for offset in range(0, len(audio), 8192):
                        chunk = audio[offset:offset+8192]
                        self.wfile.write(chunk)
                        if len(chunk) == 8192: self.wfile.write(b'\x03' + metadata)
                else: self.wfile.write(audio)
                self.wfile.flush()
            if ext.endswith('chain'):
                import time
                time.sleep(4)  # avoid reconnect masking a broken chain reader
        except (BrokenPipeError, ConnectionResetError): pass
    def log_message(self, fmt, *args): pass
server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
port_file.write_text(str(server.server_port))
server.serve_forever()
