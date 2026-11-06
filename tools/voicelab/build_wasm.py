"""Builds tools/voicelab/engine.wasm, the voice lab's real-time engine, from the module's own effect sources.

Needs the Zig toolchain's bundled clang: `pip install ziglang`. The committed engine.wasm lets the lab run without it;
rebuild after changing source/audio_effects.h, source/preset_format.h or tools/voicelab/*.h.
"""

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SOURCES = [HERE / "engine_wasm.cpp"]
INPUTS = SOURCES + [ROOT / "source" / "audio_effects.h", ROOT / "source" / "preset_format.h", HERE / "gvox_writer.h"]
OUTPUT = HERE / "engine.wasm"


def is_stale():
    """Returns whether engine.wasm is missing or older than anything it is built from."""
    return not OUTPUT.exists() or any(path.stat().st_mtime > OUTPUT.stat().st_mtime for path in INPUTS)


def build():
    """Compiles the engine as a WASI reactor with no exceptions; raises with the compiler output on failure."""
    command = [
        sys.executable, "-m", "ziglang", "c++", "-target", "wasm32-wasi", "-O2", "-fno-exceptions",
        "-mexec-model=reactor", "-s", "-I", str(ROOT / "source"), "-I", str(HERE),
        *map(str, SOURCES), "-o", str(OUTPUT),
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("engine.wasm build failed:\n" + result.stderr)


if __name__ == "__main__":
    try:
        build()
    except RuntimeError as error:
        print(error, file=sys.stderr)
        sys.exit(1)
    print(f"built {OUTPUT} ({OUTPUT.stat().st_size} bytes)")
