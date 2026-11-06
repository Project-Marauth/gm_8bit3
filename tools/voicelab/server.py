"""Voice lab: a local page for auditioning and tuning eightbit presets without building or loading the server DLL.

Run `python tools/voicelab/server.py` from the repository root (or anywhere) and open http://127.0.0.1:8765.
The page records or loads a voice line and plays it back through the module's real effect engine: the server
compiles tools/voicelab/voicelab.cpp against source/ with a local C++17 compiler (g++ by default, override with
the CXX environment variable) and rebuilds it whenever the engine sources change.
"""

import base64
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SOURCES = [HERE / "voicelab.cpp", ROOT / "source" / "voice_presets.cpp"]
HEADERS = [ROOT / "source" / "audio_effects.h", ROOT / "source" / "voice_presets.h"]
BUILD = HERE / "build"
BINARY = BUILD / ("voicelab.exe" if os.name == "nt" else "voicelab")
HOST = "127.0.0.1"
PORT = int(os.environ.get("VOICELAB_PORT", "8765"))
MAX_UPLOAD = 32 * 1024 * 1024

build_lock = threading.Lock()


def needs_build():
    """Returns whether the renderer is missing or older than any engine source."""
    if not BINARY.exists():
        return True
    built = BINARY.stat().st_mtime
    return any(path.stat().st_mtime > built for path in SOURCES + HEADERS)


def build():
    """Compiles the renderer when the engine changed, so edits to audio_effects.h show up on the next render."""
    with build_lock:
        if not needs_build():
            return
        compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            raise RuntimeError("no C++ compiler found; install g++ or set CXX")
        BUILD.mkdir(exist_ok=True)
        command = [compiler, "-std=c++17", "-O2", "-I", str(ROOT / "source"), *map(str, SOURCES), "-o", str(BINARY)]
        print("[voicelab] building:", " ".join(command), flush=True)
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError("renderer build failed:\n" + result.stderr)


def presets():
    """Returns the built-in catalogue, each preset with its full .gvox text."""
    build()
    result = subprocess.run([str(BINARY), "list"], capture_output=True, text=True, check=True)
    return result.stdout


def render(payload):
    """Renders a chain over the posted WAV and returns the processed WAV bytes."""
    build()
    with tempfile.TemporaryDirectory(prefix="voicelab-") as work:
        work = Path(work)
        source = work / "in.wav"
        output = work / "out.wav"
        source.write_bytes(base64.b64decode(payload["wav"]))

        stages = []
        for index, stage in enumerate(payload.get("stages", [])):
            if isinstance(stage, dict) and isinstance(stage.get("gvox"), str):
                path = work / f"stage{index}.gvox"
                path.write_text(stage["gvox"], encoding="utf-8")
                stages.append(str(path))
            elif isinstance(stage, int):
                stages.append(f"builtin:{stage}")

        result = subprocess.run([str(BINARY), "render", str(source), str(output), *stages], capture_output=True, text=True)
        if result.returncode != 0:
            raise ValueError(result.stderr.strip() or "render failed")
        return output.read_bytes()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def reply(self, status, body, content_type):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def fail(self, status, message):
        self.reply(status, json.dumps({"error": message}).encode(), "application/json")

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self.reply(200, (HERE / "index.html").read_bytes(), "text/html; charset=utf-8")
        elif self.path == "/api/presets":
            try:
                self.reply(200, presets().encode(), "application/json")
            except Exception as error:
                self.fail(500, str(error))
        else:
            self.fail(404, "not found")

    def do_POST(self):
        if self.path != "/api/render":
            return self.fail(404, "not found")
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > MAX_UPLOAD:
            return self.fail(413, "upload too large")
        try:
            payload = json.loads(self.rfile.read(length))
            self.reply(200, render(payload), "audio/wav")
        except ValueError as error:
            self.fail(400, str(error))
        except Exception as error:
            self.fail(500, str(error))


def main():
    try:
        build()
    except RuntimeError as error:
        print(f"[voicelab] {error}", file=sys.stderr)
        return 1
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"[voicelab] open http://{HOST}:{PORT}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
