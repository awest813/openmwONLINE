#!/usr/bin/env python3
"""
OpenMW WASM local test server.

Serves the WASM build output with the HTTP headers required for
cross-origin isolation (needed by the pthread/SharedArrayBuffer build):

    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

Usage:
    python3 scripts/serve_wasm.py <build_dir> [--port PORT] [--no-coep]

Arguments:
    build_dir       Directory containing openmw.html, openmw.js, openmw.wasm.
                    Defaults to the current working directory.
    --port PORT     TCP port to listen on (default: 8080).
    --no-coep       Omit COOP/COEP headers (non-pthread build, any server works).

Examples:
    # Serve a pthread build on port 8080 (COOP/COEP headers enabled)
    python3 scripts/serve_wasm.py /path/to/build

    # Serve a non-pthread build without isolation headers
    python3 scripts/serve_wasm.py /path/to/build --no-coep

    # Custom port
    python3 scripts/serve_wasm.py /path/to/build --port 9000
"""

import argparse
import http.server
import os
import sys
from functools import partial


# MIME types for WASM and related assets
_EXTRA_TYPES = {
    ".wasm": "application/wasm",
    ".js": "application/javascript",
    ".html": "text/html; charset=utf-8",
    ".md": "text/markdown; charset=utf-8",
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
        description="Serve the OpenMW WASM build with cross-origin isolation headers.",
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
        "--no-coep",
        dest="cors_headers",
        action="store_false",
        help="Disable COOP/COEP headers (use for non-pthread builds)",
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

    handler = partial(
        WASMRequestHandler,
        cors_headers=args.cors_headers,
        directory=build_dir,
    )

    url = f"http://localhost:{args.port}/openmw.html"
    headers_note = (
        "with COOP/COEP headers (pthread build)"
        if args.cors_headers
        else "without COOP/COEP headers (non-pthread build)"
    )

    print(f"Serving {build_dir}")
    print(f"  {headers_note}")
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
