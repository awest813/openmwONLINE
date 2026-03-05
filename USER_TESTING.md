# OpenMW WASM — User Testing Guide

This guide is for people who want to help test the experimental
**WebAssembly (browser) build** of OpenMW. Your feedback is invaluable
for catching bugs before a wider release.

---

## Prerequisites

| Requirement | Details |
|---|---|
| **Morrowind** | You must own Morrowind. The WASM build does not include game data. |
| **Browser** | Chrome 86+ or Edge 86+ recommended (native directory picker). Firefox 111+ is supported with a directory-upload fallback. Safari is not supported. |
| **WASM build** | `openmw.html`, `openmw.js`, and `openmw.wasm` produced by the CI job or a local build. |
| **HTTP server** | Required (browsers block WASM from `file://`). See [Serving locally](#serving-locally) below. |

---

## Serving locally

A helper script is included at `scripts/serve_wasm.py`. It starts a
local HTTP server with the cross-origin isolation headers required by
the **pthread build** (`COOP`/`COEP`). The non-pthread build works with
any server.

```bash
# Serve the WASM build output (default: port 8080)
python3 scripts/serve_wasm.py /path/to/wasm/build/output

# Specify a port
python3 scripts/serve_wasm.py /path/to/wasm/build/output --port 9000
```

Then open `http://localhost:8080/openmw.html` in your browser.

If you prefer a one-liner without the helper script:

```bash
# Non-pthread build only (no COOP/COEP required)
python3 -m http.server 8080 --directory /path/to/wasm/build/output
```

---

## Step-by-step testing checklist

Work through these in order. Note your result (pass / fail / partial)
and any console errors for each item.

### Phase 1 — Engine startup

- [ ] Page loads in Chrome without JS console errors before the engine initializes
- [ ] Loading progress bar advances and reaches 100%
- [ ] Status text changes from "Initializing engine..." to "Ready - select your Morrowind data folder to begin"
- [ ] "Select Morrowind Data Folder" button appears

### Phase 2 — Data loading

- [ ] Clicking the button opens a directory picker (Chrome/Edge) or directory-upload fallback (Firefox)
- [ ] Selecting the `Data Files` folder starts the upload phase
- [ ] Scanning phase shows "Counting files..."
- [ ] Upload progress bar updates smoothly during file transfer
- [ ] File count and byte count displayed match what you expect (Morrowind vanilla ≈ 700 MB)
- [ ] After upload completes, status reads "Game data loaded successfully!"
- [ ] Cancelling the picker restores the button correctly (does not leave it disabled)

### Phase 3 — Rendering and main menu

- [ ] After data loads, the loading overlay fades out and the canvas becomes visible
- [ ] "Click to Play" overlay appears on the canvas
- [ ] Clicking the overlay engages pointer lock and hides the overlay
- [ ] The main menu renders (background, logo, menu options are visible)
- [ ] Menu background is solid black (non-pthread build — expected) or plays the cinematic (pthread build)
- [ ] No visible shader corruption, black meshes, or Z-fighting


### Phase 3.5 — HDR / water fallback validation

- [ ] Capture browser console lines beginning with `WASM:` that describe ripple/luminance format choice
- [ ] Confirm scene still renders when float color buffers are unavailable (no black screen / no crash)
- [ ] Compare water ripples against a float-capable browser to verify no severe artifacts (minor precision loss acceptable)
- [ ] Compare HDR adaptation speed/brightness against a float-capable browser (banding tolerable, broken exposure is not)

### Phase 4 — Input and interaction

- [ ] Mouse movement is captured and controls the camera/cursor correctly
- [ ] Pressing Escape releases pointer lock
- [ ] Clicking the canvas after releasing pointer lock re-engages pointer lock
- [ ] Keyboard input reaches the game (typing works in the console, etc.)

### Phase 5 — New game / save / load

- [ ] "New Game" starts the character creation sequence
- [ ] Character creation UI renders correctly (no layout issues)
- [ ] Starting a new game places you in the game world (Seyda Neen)
- [ ] The world renders: terrain, objects, sky, and NPCs are visible
- [ ] No persistent black triangles or missing geometry
- [ ] Saving a game completes without error (check browser console)
- [ ] Reloading the page and re-selecting data, then loading the save works
- [ ] Loaded game restores position and inventory correctly

### Phase 6 — Performance

- [ ] Frame rate is playable (≥ 20 fps) in an open area such as Seyda Neen
- [ ] Frame rate is reasonable indoors (Caius Cosades' house)
- [ ] No visible hitching on asset loads after the first few seconds
- [ ] Browser tab memory stays below 3 GB during a typical 10-minute session

### Phase 7 — Persistence

- [ ] Saving the game, then closing and reopening the tab loads your saves
- [ ] Config settings (e.g., changing graphics options) persist across reloads

### Phase 8 — Console overlay

- [ ] Pressing `Ctrl+\`` (Ctrl + backtick) toggles the console overlay
- [ ] Logs, warnings, and errors appear in the overlay during gameplay
- [ ] "Copy Log" button copies the full log to the clipboard
- [ ] Pressing "Copy Log" and pasting produces the full session log

---

## Reporting a bug

Please include the following in every bug report:

1. **Browser and version** (e.g., Chrome 122.0.6261.112)
2. **OS** (Windows 11, macOS 14, Ubuntu 24.04, etc.)
3. **Build type**: pthread or non-pthread (check URL or build notes)
4. **Steps to reproduce** — what you did before the problem occurred
5. **Expected result** vs **Actual result**
6. **Console log** — use "Copy Log" in the console overlay (`Ctrl+\``) and paste it

**Where to report:**
- GitLab Issues: https://gitlab.com/OpenMW/openmw/-/issues
- Use the labels `wasm` and `bug`

### Issue template

```
**Browser:** Chrome 122 / Firefox 124 / Edge 122
**OS:** Windows 11
**Build type:** non-pthread
**Phase:** Phase 3 — Rendering and main menu

**Steps to reproduce:**
1. Loaded Data Files folder (vanilla Morrowind, ~700 MB)
2. Clicked "Click to Play"

**Expected:** Main menu renders with background image

**Actual:** Canvas is black; no geometry visible

**Console log:**
<paste log here>
```

---

## Known issues and limitations

| Issue | Details |
|---|---|
| Menu background video absent | Only plays in the pthread build. Non-pthread build shows a black screen — expected. |
| HDR/ripple precision may downgrade by capability | Runtime picks best available path: float (`EXT_color_buffer_float`) when possible, ripple half-float fallback (`EXT_color_buffer_half_float`) otherwise, and full 8-bit fallback when neither extension is present. |
| Compute shaders disabled | Water ripple compute pass is disabled. Ripples use CPU-side fallback. |
| Safari not supported | Missing required directory import APIs. No workaround currently. |
| Initial load is slow | First data upload can take 1–5 minutes for a vanilla install. Subsequent sessions re-use data from IndexedDB. |
| EFX/reverb audio absent | Web Audio handles spatialization natively; EFX reverb effects are disabled. |
| Mod support untested | Mod files uploaded via the directory picker should work, but this is not yet validated. |

---

## Pthread vs non-pthread builds

The pthread build requires cross-origin isolation headers from the HTTP
server. The `scripts/serve_wasm.py` helper sets these automatically.

| Feature | Non-pthread | Pthread |
|---|---|---|
| Background video | No | Yes |
| Multithreaded physics | No | Yes |
| NavMesh background calc | No | Yes |
| Server COOP/COEP headers | Not required | Required |

---

## Tips for testers

- Keep the browser developer tools open (`F12`) and monitor the Console
  and Memory tabs during your session.
- Use the console overlay (`Ctrl+\``) to watch engine log output without
  leaving the game window.
- Test saves after every major action (entering a new cell, completing
  dialogue, equipping items) so that regression bugs are easy to
  pinpoint.
- Try both a **vanilla Morrowind** install (no mods) and, if you have
  one, a **lightly modded** install to check mod compatibility.
- If the page crashes or freezes, take a screenshot of the browser's
  crash/hang dialog and note what you were doing at the time.


## Browser capability matrix (HDR/ripple fallback)

| Browser/runtime capability | Ripple format | Luminance format | Outcome |
|---|---|---|---|
| `EXT_color_buffer_float` present | `GL_RGBA16F` | `GL_R16F` | Highest quality path |
| Float absent, `EXT_color_buffer_half_float` present | `GL_RGBA16F` | `GL_R8` | Ripple quality improved, HDR precision reduced |
| Neither extension present | `GL_RGBA8` | `GL_R8` | Lowest quality, but rendering must remain stable |

When reporting bugs, include the `WASM:` capability logs from the in-game console overlay so we can correlate visual quality with the selected fallback path.
