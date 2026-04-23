#!/usr/bin/env bash
# --------------------------------------------------------------------------
# package_wasm.sh — Bundle the OpenMW WASM build into a deployable archive.
#
# Produces a self-contained ZIP archive that includes:
#   - The three required WASM artifacts:
#       openmw.html   openmw.js   openmw.wasm   openmw.data (if present)
#   - Deployment configs for nginx, Netlify, Vercel, and Cloudflare Pages.
#   - A README with quick-start instructions.
#
# This archive can be dropped onto any supported static host or extracted
# onto a server and served with the bundled nginx config.
#
# Usage:
#   scripts/package_wasm.sh [--build-dir DIR] [--out-dir DIR] [--pthread]
#
# Options:
#   --build-dir DIR   Directory containing openmw.html / .js / .wasm
#                     (default: ./build-wasm or $BUILD_DIR)
#   --out-dir DIR     Where to write the output ZIP
#                     (default: same as --build-dir)
#   --pthread         Tag the archive as a pthread build and include
#                     the COOP/COEP headers in the README notice.
#                     Without this flag the archive is tagged non-pthread.
#
# Output:
#   openmw-wasm-<build>.zip          Deployable archive
#   openmw-wasm-<build>.zip.sha256   SHA-256 checksum
# --------------------------------------------------------------------------

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEPLOY_DIR="$ROOT_DIR/deploy"

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-wasm}"
OUT_DIR=""
PTHREAD=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)  BUILD_DIR="$2"; shift 2 ;;
        --out-dir)    OUT_DIR="$2";   shift 2 ;;
        --pthread)    PTHREAD=true;   shift   ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Usage: $0 [--build-dir DIR] [--out-dir DIR] [--pthread]" >&2
            exit 1
            ;;
    esac
done

BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
OUT_DIR="${OUT_DIR:-$BUILD_DIR}"
mkdir -p "$OUT_DIR"

if $PTHREAD; then
    BUILD_TAG="pthread"
else
    BUILD_TAG="no-threads"
fi

ARCHIVE_NAME="openmw-wasm-${BUILD_TAG}.zip"
ARCHIVE_PATH="$OUT_DIR/$ARCHIVE_NAME"
WORK_DIR="$(mktemp -d)"
STAGE_DIR="$WORK_DIR/openmw-wasm-${BUILD_TAG}"
mkdir -p "$STAGE_DIR"

# ──────────────────────────────────────────────────────────────────────────
# 1. Copy WASM build artifacts
# ──────────────────────────────────────────────────────────────────────────
echo "=== Collecting WASM artifacts from $BUILD_DIR ==="

for artifact in openmw.html openmw.js openmw.wasm; do
    if [ ! -f "$BUILD_DIR/$artifact" ]; then
        echo "error: required artifact not found: $BUILD_DIR/$artifact" >&2
        echo "       Build the WASM target first with:" >&2
        echo "       CI/before_script.wasm.sh && cd build-wasm && emmake make -j\$(nproc)" >&2
        rm -rf "$WORK_DIR"
        exit 1
    fi
    cp "$BUILD_DIR/$artifact" "$STAGE_DIR/"
done

# openmw.data is optional (present when --preload-file assets are used)
if [ -f "$BUILD_DIR/openmw.data" ]; then
    cp "$BUILD_DIR/openmw.data" "$STAGE_DIR/"
fi

# ──────────────────────────────────────────────────────────────────────────
# 2. Copy deployment configs
# ──────────────────────────────────────────────────────────────────────────
echo "=== Bundling deployment configs ==="

mkdir -p "$STAGE_DIR/deploy"

if $PTHREAD; then
    # pthread build — all configs including COOP/COEP headers
    cp "$DEPLOY_DIR/nginx.conf"    "$STAGE_DIR/deploy/"
    cp "$DEPLOY_DIR/netlify.toml"  "$STAGE_DIR/deploy/"
    cp "$DEPLOY_DIR/_headers"      "$STAGE_DIR/deploy/"
    cp "$DEPLOY_DIR/vercel.json"   "$STAGE_DIR/deploy/"
else
    # non-pthread build — configs without COOP/COEP headers
    cp "$DEPLOY_DIR/nginx-no-threads.conf"    "$STAGE_DIR/deploy/nginx.conf"
    cp "$DEPLOY_DIR/netlify-no-threads.toml"  "$STAGE_DIR/deploy/netlify.toml"
    cp "$DEPLOY_DIR/vercel-no-threads.json"   "$STAGE_DIR/deploy/vercel.json"
    # _headers not needed for non-pthread; omitted intentionally
fi

# ──────────────────────────────────────────────────────────────────────────
# 3. Write a quick-start README into the archive
# ──────────────────────────────────────────────────────────────────────────
echo "=== Writing quick-start README ==="

if $PTHREAD; then
THREAD_NOTE="**pthread build** — requires cross-origin isolation headers.
All configs in \`deploy/\` already include the required headers.
Do NOT serve this build from a host that cannot set custom HTTP headers
(e.g. plain GitHub Pages without a Cloudflare proxy)."
SERVER_CMD="python3 serve_wasm.py . --port 8080"
else
THREAD_NOTE="**non-pthread (single-threaded) build** — works with any HTTP server.
No special headers are required. Background video and multi-threaded physics
are not available in this build."
SERVER_CMD="python3 serve_wasm.py . --port 8080"
fi

cat > "$STAGE_DIR/README.md" <<EOREADME
# OpenMW WASM — Deployable Package (${BUILD_TAG})

${THREAD_NOTE}

## Quick start

### Local test server

\`\`\`bash
# Python local server (included in the archive; auto-detects pthread vs non-pthread)
${SERVER_CMD}
# Then open: http://localhost:8080/openmw.html
\`\`\`

### nginx

Copy \`openmw.html\`, \`openmw.js\`, \`openmw.wasm\` (and \`openmw.data\` if present)
to your nginx root, then install the bundled nginx config:

\`\`\`bash
sudo cp deploy/nginx.conf /etc/nginx/sites-available/openmw-wasm
sudo ln -s /etc/nginx/sites-available/openmw-wasm /etc/nginx/sites-enabled/
# Edit server_name and root inside the file first, then:
sudo nginx -t && sudo systemctl reload nginx
\`\`\`

### Netlify (drag-and-drop)

1. Go to https://app.netlify.com/drop
2. Drag this entire folder onto the page.
3. Done. The \`deploy/netlify.toml\` is picked up automatically.

### Cloudflare Pages / Netlify (_headers)

Deploy via Git or CLI and ensure \`deploy/_headers\` is in your published root.

### Vercel

Copy \`deploy/vercel.json\` to the root of your Vercel project, then run:

\`\`\`bash
vercel deploy --prod
\`\`\`

## Files in this package

| File | Description |
|---|---|
| \`openmw.html\` | Entry point — open this in a browser |
| \`openmw.js\`   | Emscripten JS glue code |
| \`openmw.wasm\` | WebAssembly binary |
| \`openmw.data\` | Preloaded assets (if present) |
| \`deploy/nginx.conf\` | nginx server block |
| \`deploy/netlify.toml\` | Netlify build + headers config |
| \`deploy/_headers\` | Cloudflare Pages / Netlify \`_headers\` format |
| \`deploy/vercel.json\` | Vercel headers config |

## Header requirements

| Header | pthread build | non-pthread build |
|---|---|---|
| \`Cross-Origin-Opener-Policy: same-origin\` | **Required** | Not needed |
| \`Cross-Origin-Embedder-Policy: require-corp\` | **Required** | Not needed |

See [DEPLOYMENT.md](https://github.com/awest813/openmwONLINE/blob/master/DEPLOYMENT.md)
for full deployment documentation.
EOREADME

# ──────────────────────────────────────────────────────────────────────────
# 4. Bundle local dev server scripts for convenience
# ──────────────────────────────────────────────────────────────────────────
cp "$ROOT_DIR/scripts/serve_wasm.py" "$STAGE_DIR/"
cp "$ROOT_DIR/scripts/serve_wasm_detect.py" "$STAGE_DIR/"

# ──────────────────────────────────────────────────────────────────────────
# 5. Create the ZIP archive
# ──────────────────────────────────────────────────────────────────────────
echo "=== Creating archive $ARCHIVE_PATH ==="

(cd "$WORK_DIR" && zip -r "$ARCHIVE_PATH" "openmw-wasm-${BUILD_TAG}/")

# SHA-256 checksum
sha256sum "$ARCHIVE_PATH" > "${ARCHIVE_PATH}.sha256"

rm -rf "$WORK_DIR"

echo ""
echo "=================================================================="
echo "Package created: $ARCHIVE_PATH"
echo "Checksum:        ${ARCHIVE_PATH}.sha256"
echo ""
echo "Build type: ${BUILD_TAG}"
if $PTHREAD; then
    echo "  → Requires COOP/COEP headers on the hosting server."
    echo "  → Use deploy/nginx.conf, deploy/netlify.toml, deploy/_headers,"
    echo "    or deploy/vercel.json — all include the required headers."
else
    echo "  → Works on any static file host without special headers."
    echo "  → GitHub Pages, S3, Netlify free tier, or any CDN will work."
fi
echo "=================================================================="
