"""
Heuristics to detect whether an Emscripten OpenMW build uses pthreads.

Pthread builds need Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy
so SharedArrayBuffer works. Single-threaded builds work with a plain static
server; sending COOP/COEP is unnecessary and can confuse users who copy
commands from the wrong section of the docs.
"""

from __future__ import annotations

import os

# Emscripten pthread glue appears early in openmw.js; a bounded read avoids
# loading multi-megabyte bundles into memory.
_READ_BYTES = 512 * 1024

# Substrings observed in Emscripten's pthread-enabled runtime output.
_PTHREAD_MARKERS = (
    "var PThread",
    "PThread.init",
    "invokeEntryPoint",
    "_emscripten_proxy_main",
    "PTHREAD_POOL_SIZE",
    "allocateUnusedWorker",
)


def detect_pthread_openmw_js(build_dir: str) -> bool | None:
    """
    Return True if openmw.js in build_dir appears to be a pthread build,
    False if it appears single-threaded, None if openmw.js is missing or
    unreadable (caller should pick a safe default).
    """
    js_path = os.path.join(build_dir, "openmw.js")
    if not os.path.isfile(js_path):
        return None
    try:
        with open(js_path, "rb") as f:
            chunk = f.read(_READ_BYTES)
    except OSError:
        return None
    if not chunk:
        return None
    text = chunk.decode("utf-8", errors="ignore")
    return any(m in text for m in _PTHREAD_MARKERS)
