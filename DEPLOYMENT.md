# OpenMW WASM — Deployment Guide

This guide explains how to host the OpenMW WebAssembly build so that anyone
with a copy of Morrowind can play in their browser. It covers **every supported
hosting target**, the exact HTTP headers required for each build type, and how
to obtain or produce the deployable artifact package.

> **No build required.** The CI job produces a ready-to-extract ZIP.
> Download the latest artifact from the CI pipeline and follow the steps for
> your chosen host.

---

## Build types and header requirements

The OpenMW WASM build comes in two flavours.  **Choose the one that matches
your hosting constraints before you deploy.**

| Feature | non-pthread build | pthread build |
|---|---|---|
| Multi-threaded physics | ✗ | ✅ |
| NavMesh background worker | ✗ | ✅ |
| Background intro video | ✗ | ✅ |
| Special HTTP headers required | **No** | **Yes** (see below) |
| Works on GitHub Pages | ✅ Yes | ✗ Without extra config |
| Works on Netlify free tier | ✅ Yes | ✅ Yes (with `_headers` or `netlify.toml`) |
| Works on Cloudflare Pages | ✅ Yes | ✅ Yes (with `_headers`) |
| Works on Vercel | ✅ Yes | ✅ Yes (with `vercel.json`) |
| Works on any nginx server | ✅ Yes | ✅ Yes (with `deploy/nginx.conf`) |

### Required headers for the pthread build

The browser's `SharedArrayBuffer` API — needed by Emscripten's pthread runtime
— is only available in **cross-origin isolated** contexts.  Your server must
send both of these headers on every response:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

**The non-pthread build does not need these headers.** You can serve it from
any static file host that can serve the correct MIME type for `.wasm` files
(`application/wasm`).

---

## Obtaining the build artifacts

### From CI

The `Emscripten_WASM` GitLab CI job (or the `wasm` GitHub Actions job) produces
a deployable ZIP archive as an artifact:

```
openmw-wasm-no-threads.zip   ← non-pthread build
openmw-wasm-pthread.zip      ← pthread build (requires headers)
```

Each archive contains:

```
openmw-wasm-<build>/
├── openmw.html          ← Open this in a browser
├── openmw.js
├── openmw.wasm
├── openmw.data          ← Present if --preload-file assets were included
├── serve_wasm.py        ← Local dev server (Python, cross-platform)
├── README.md            ← Quick-start cheat sheet
└── deploy/
    ├── nginx.conf       ← nginx server block
    ├── netlify.toml     ← Netlify build + headers config
    ├── _headers         ← Cloudflare Pages / Netlify _headers format
    └── vercel.json      ← Vercel headers config
```

### Building locally

```bash
# Non-pthread build (works with any HTTP server)
CI/before_script.wasm.sh
cd build-wasm && emmake make -j$(nproc)

# Package it
scripts/package_wasm.sh --build-dir build-wasm

# pthread build (requires COOP/COEP headers)
# Edit CI/before_script.wasm.sh and set -DOPENMW_EXPERIMENTAL_WASM_PTHREADS=ON
CI/before_script.wasm.sh
cd build-wasm && emmake make -j$(nproc)
scripts/package_wasm.sh --build-dir build-wasm --pthread
```

---

## Hosting recipes

### Option 1 — Local development server (Python)

The quickest way to test a local build on your own machine.

**Non-pthread build — any HTTP server:**

```bash
python3 -m http.server 8080 --directory /path/to/build-wasm
# Open: http://localhost:8080/openmw.html
```

**Pthread build — requires COOP/COEP headers:**

```bash
python3 scripts/serve_wasm.py /path/to/build-wasm --port 8080
# Open: http://localhost:8080/openmw.html

# Non-pthread build via the same script (headers disabled):
python3 scripts/serve_wasm.py /path/to/build-wasm --port 8080 --no-coep
```

The helper script is also bundled in every CI artifact package.

---

### Option 2 — nginx (self-hosted server)

**Step 1: Copy build artifacts**

```bash
sudo mkdir -p /var/www/openmw-wasm
sudo cp openmw.html openmw.js openmw.wasm /var/www/openmw-wasm/
# If present:
sudo cp openmw.data /var/www/openmw-wasm/
```

**Step 2: Install the config**

```bash
# For the pthread build:
sudo cp deploy/nginx.conf /etc/nginx/sites-available/openmw-wasm

# For the non-pthread build:
sudo cp deploy/nginx-no-threads.conf /etc/nginx/sites-available/openmw-wasm
```

**Step 3: Edit the config**

Open `/etc/nginx/sites-available/openmw-wasm` and replace:

- `server_name example.com;` → your actual domain
- `root /var/www/openmw-wasm;` → path where you copied the files
- SSL certificate paths (if using HTTPS)

**Step 4: Enable and reload**

```bash
sudo ln -sf /etc/nginx/sites-available/openmw-wasm \
             /etc/nginx/sites-enabled/openmw-wasm
sudo nginx -t && sudo systemctl reload nginx
```

**Verify headers** (pthread build only):

```bash
curl -I https://your-domain.com/openmw.html | grep -i cross-origin
# Expected output:
#   Cross-Origin-Opener-Policy: same-origin
#   Cross-Origin-Embedder-Policy: require-corp
```

---

### Option 3 — Netlify

**Drag-and-drop (quickest):**

1. Extract the CI artifact ZIP.
2. Open https://app.netlify.com/drop.
3. Drag the extracted folder (`openmw-wasm-<build>/`) onto the page.
4. Netlify picks up `deploy/netlify.toml` and sets headers automatically.

**CLI deploy:**

```bash
npm install -g netlify-cli
netlify deploy --dir /path/to/openmw-wasm-<build> --prod
```

**Git-connected site:**

Add `deploy/netlify.toml` to your repository root (or copy it to wherever
your `publish` directory is configured) and push. Netlify applies the headers
on the next build.

> For the **non-pthread build**: headers are not required. You can deploy
> without any `netlify.toml` or use the stripped-down version from the
> non-pthread artifact package.

---

### Option 4 — Cloudflare Pages

1. Create a new Pages project (connect a Git repo or upload directly).
2. Copy `deploy/_headers` to the root of your published directory.
3. Deploy.

Cloudflare Pages reads the `_headers` file and applies the COOP/COEP headers
to every response.

**Verify after deployment:**

```bash
curl -I https://your-pages-domain.pages.dev/openmw.html | grep -i cross-origin
```

> For the **non-pthread build**: no `_headers` file is needed. Deploy the
> three WASM files directly.

---

### Option 5 — Vercel

1. Create a Vercel project.
2. Copy `deploy/vercel.json` to the root of your project.
3. Push or run `vercel deploy --prod`.

**vercel.json** in the artifact sets COOP/COEP on every route. For the
non-pthread build, the CI packaging script strips those headers from the
bundled `vercel.json` automatically.

---

### Option 6 — GitHub Pages (non-pthread build only)

GitHub Pages does not support custom HTTP response headers, so the pthread
build **cannot** be used here without a Cloudflare proxy.

The **non-pthread build** works fine:

1. Push `openmw.html`, `openmw.js`, and `openmw.wasm` to a `gh-pages` branch
   (or configure GitHub Pages to serve from `docs/` or the repo root).
2. Enable GitHub Pages in the repo Settings → Pages.
3. Open `https://<user>.github.io/<repo>/openmw.html`.

---

## Verifying cross-origin isolation

After deploying the pthread build, confirm that cross-origin isolation is
active in the browser:

```javascript
// Open the browser console on your deployed page and run:
crossOriginIsolated  // should print: true
typeof SharedArrayBuffer !== 'undefined'  // should print: true
```

If `crossOriginIsolated` is `false`, the COOP/COEP headers are missing or
incorrect. Check your server config and use `curl -I` to inspect the actual
response headers.

---

## MIME types

All hosting configs in `deploy/` set the correct MIME types. If you are
configuring a host not listed here, ensure these types are registered:

| Extension | MIME type |
|---|---|
| `.wasm` | `application/wasm` |
| `.js` | `application/javascript` |
| `.html` | `text/html; charset=utf-8` |
| `.data` | `application/octet-stream` |

---

## Caching recommendations

| File | Recommended Cache-Control |
|---|---|
| `openmw.html` | `no-cache, no-store, must-revalidate` |
| `openmw.js` | `no-cache, no-store, must-revalidate` |
| `openmw.wasm` | `public, max-age=31536000, immutable` |
| `openmw.data` | `public, max-age=31536000, immutable` |

The HTML and JS files change with every build; cache them for zero seconds.
The WASM and data files are produced from a deterministic build and can be
cached for up to a year.

---

## Troubleshooting

### "SharedArrayBuffer is not defined"

The pthread build requires cross-origin isolation. Verify that both
`Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp` are present in the response
headers. See [Verifying cross-origin isolation](#verifying-cross-origin-isolation).

### "TypeError: Failed to fetch" loading .wasm

The server is either returning the wrong MIME type for `.wasm` files or the
WASM file is not being found. Check your nginx/host config and confirm the
file is at the expected path.

### The engine loads but immediately crashes

Check the browser console for error messages. The most common cause is missing
Morrowind data files — you must load your own copy via the "Select Morrowind
Data Folder" button.

### Page loads but canvas stays black

Try the non-pthread build — the pthread build shows a solid black background
in the main menu (no background video) only when cross-origin isolation is
missing. Use the [verification steps](#verifying-cross-origin-isolation) above.

---

## Further reading

- [WASM_ROADMAP.md](WASM_ROADMAP.md) — technical status and architecture
- [USER_TESTING.md](USER_TESTING.md) — structured testing checklist
- [scripts/serve_wasm.py](scripts/serve_wasm.py) — local dev server source
- [scripts/package_wasm.sh](scripts/package_wasm.sh) — artifact packaging script
