#!/usr/bin/env python3
"""
OpenMW WASM local test server.

Can send the HTTP headers required for cross-origin isolation (needed by the
pthread / SharedArrayBuffer build):

    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

By default, COOP/COEP are chosen automatically from openmw.js (pthread vs
single-threaded). Override with --coep on|off if detection is wrong.

Usage:
    python3 scripts/serve_wasm.py <build_dir> [--port PORT] [--coep MODE]

Arguments:
    build_dir       Directory containing openmw.html, openmw.js, openmw.wasm.
                    Defaults to the current working directory.
    --port PORT     TCP port to listen on (default: 8080).
    --coep MODE     Cross-origin isolation headers: auto (default), on, or off.
    --no-coep       Same as --coep off (kept for scripts and muscle memory).

Examples:
    # Auto-detect pthread vs non-pthread from openmw.js (recommended)
    python3 scripts/serve_wasm.py /path/to/build

    # Force headers on (pthread / SharedArrayBuffer build)
    python3 scripts/serve_wasm.py /path/to/build --coep on

    # Force headers off (plain static server behaviour)
    python3 scripts/serve_wasm.py /path/to/build --coep off

    # Custom port
    python3 scripts/serve_wasm.py /path/to/build --port 9000
"""

import argparse
import http.server
import os
import sys
from functools import partial

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

from serve_wasm_detect import detect_pthread_openmw_js


# MIME types for WASM and related assets
_EXTRA_TYPES = {
    ".wasm": "application/wasm",
    ".js": "application/javascript",
    ".html": "text/html; charset=utf-8",
    ".md": "text/markdown; charset=utf-8",
    ".data": "application/octet-stream",
}


class WASMRequestHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP handler that injects cross-origin isolation headers."""

    def __init__(self, *args, cors_headers=True, **kwargs):
        self._cors_headers = cors_headers
        super().__init__(*args, **kwargs)

    def end_headers(self):
        if self._cors_headers:
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        super().end_headers()

    def guess_type(self, path):
        ext = os.path.splitext(path)[1].lower()
        if ext in _EXTRA_TYPES:
            return _EXTRA_TYPES[ext]
        return super().guess_type(path)

    def log_message(self, fmt, *args):
        # Suppress the default per-request spam; only log notable events.
        status = args[1] if len(args) > 1 else "?"
        if str(status).startswith(("4", "5")):
            sys.stderr.write(f"[{self.log_date_time_string()}] {fmt % args}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Serve the OpenMW WASM build with optional cross-origin isolation headers.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "build_dir",
        nargs="?",
        default=".",
        help="Directory containing openmw.html (default: current directory)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="TCP port to listen on (default: 8080)",
    )
    parser.add_argument(
        "--coep",
        choices=("auto", "on", "off"),
        default="auto",
        help="Cross-origin isolation headers: auto (inspect openmw.js), on, or off (default: auto)",
    )
    parser.add_argument(
        "--no-coep",
        action="store_true",
        help="Same as --coep off (non-pthread / plain static server)",
    )
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    if not os.path.isdir(build_dir):
        sys.exit(f"error: directory not found: {build_dir}")

    entry = os.path.join(build_dir, "openmw.html")
    if not os.path.exists(entry):
        print(
            f"warning: openmw.html not found in {build_dir}\n"
            "         Are you pointing at the right build output directory?",
            file=sys.stderr,
        )

    coep_mode = "off" if args.no_coep else args.coep
    if coep_mode == "auto":
        detected = detect_pthread_openmw_js(build_dir)
        if detected is True:
            cors_headers = True
            detect_note = "pthread markers found in openmw.js"
        elif detected is False:
            cors_headers = False
            detect_note = "no pthread markers in openmw.js (single-threaded build)"
        else:
            cors_headers = False
            detect_note = "openmw.js missing or unreadable — defaulting to no COOP/COEP (use --coep on if this is a pthread build)"
    elif coep_mode == "on":
        cors_headers = True
        detect_note = "forced on (--coep on)"
    else:
        cors_headers = False
        detect_note = "forced off (--coep off or --no-coep)"

    handler = partial(
        WASMRequestHandler,
        cors_headers=cors_headers,
        directory=build_dir,
    )

    url = f"http://localhost:{args.port}/openmw.html"
    headers_note = (
        "with COOP/COEP headers (pthread / SharedArrayBuffer)"
        if cors_headers
        else "without COOP/COEP headers (single-threaded or plain server)"
    )

    print(f"Serving {build_dir}")
    print(f"  {headers_note}")
    print(f"  ({detect_note})")
    print(f"  Open: {url}")
    print("  Press Ctrl+C to stop.\n")

    try:
        with http.server.HTTPServer(("", args.port), handler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
    except OSError as exc:
        sys.exit(f"error: cannot bind to port {args.port}: {exc}")


if __name__ == "__main__":
    main()
