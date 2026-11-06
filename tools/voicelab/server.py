"""Voice lab: a local page for hearing and tuning eightbit presets live, without building or loading the server DLL.

Run `python tools/voicelab/server.py` and open http://127.0.0.1:8765. The audio runs in the browser on engine.wasm,
the module's own effect engine compiled to WebAssembly; this server only hands out the page. When `ziglang` is
installed (`pip install ziglang`) it rebuilds engine.wasm whenever the engine sources are newer, so edits to
source/audio_effects.h can be heard on the next page load.
"""

import importlib.util
import os
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import build_wasm  # noqa: E402

HOST = "127.0.0.1"
PORT = int(os.environ.get("VOICELAB_PORT", "8765"))
SERVED = {"index.html", "lab.js", "worklet.js", "engine.wasm"}


def refresh_engine():
    """Rebuilds engine.wasm when it is stale and the Zig toolchain is available; otherwise keeps the committed one."""
    if not build_wasm.is_stale():
        return
    if importlib.util.find_spec("ziglang") is None:
        print("[voicelab] engine.wasm is older than the engine sources; `pip install ziglang` to rebuild it", flush=True)
        return
    try:
        build_wasm.build()
        print("[voicelab] rebuilt engine.wasm", flush=True)
    except RuntimeError as error:
        print(f"[voicelab] {error}", file=sys.stderr, flush=True)


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm", ".js": "text/javascript"}

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(HERE), **kwargs)

    def log_message(self, format, *args):
        pass

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_GET(self):
        name = self.path.split("?", 1)[0].lstrip("/") or "index.html"
        if name not in SERVED:
            self.send_error(404)
            return
        if name in ("index.html", "engine.wasm"):
            refresh_engine()
        self.path = "/" + name
        super().do_GET()


class Server(ThreadingHTTPServer):
    """Refuses to share the port: on Windows address reuse lets a second lab bind the same port and silently shadow the first."""

    allow_reuse_address = os.name != "nt"


def main():
    refresh_engine()
    try:
        server = Server((HOST, PORT), Handler)
    except OSError as error:
        print(f"[voicelab] cannot listen on {HOST}:{PORT} ({error}); is another voice lab running? Set VOICELAB_PORT to use another port.", file=sys.stderr)
        return 1
    print(f"[voicelab] open http://{HOST}:{PORT}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
