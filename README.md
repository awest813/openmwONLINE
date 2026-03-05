OpenMW — Play Morrowind in Your Browser
=======================================

OpenMW is an open-source game engine that brings **The Elder Scrolls III:
Morrowind** to modern platforms — including your **web browser**. No plugins, no
installs — just open a page, point it at your Morrowind data, and play.

> **🎮 The goal of this project is a fully playable Morrowind experience running
> entirely in a modern web browser.** All main quests (Morrowind, Tribunal,
> Bloodmoon) are completable. The engine compiles to WebAssembly via Emscripten,
> renders with WebGL 2.0, and persists saves to IndexedDB — everything stays in
> the browser.

You must **own Morrowind** to play. The engine does not include any game data.

* Version: 0.51.0
* License: GPLv3 (see [LICENSE](https://gitlab.com/OpenMW/openmw/-/raw/master/LICENSE))
* Website: https://www.openmw.org
* Discord: https://discord.gg/bWuqq2e
* IRC: #openmw on irc.libera.chat

---

Play in Your Browser
--------------------

### Quick start

1. **Build** the WASM target (see [Building for the browser](#building-for-the-browser) below)
   to produce `openmw.html`, `openmw.js`, and `openmw.wasm`.
2. **Serve** the build output over HTTP (any static server works for the
   non-pthread build; `python3 -m http.server` is enough).
3. **Open** `openmw.html` in Chrome, Edge, or Firefox.
4. **Select your Morrowind Data Files folder** when prompted.
5. **Click the canvas** to lock the mouse and start playing.

Your saves and settings are stored in the browser via IndexedDB and survive
page reloads.

### Browser compatibility

| Browser | Support |
|---------|---------|
| **Chrome / Edge 86+** | ✅ Full support — native directory picker, WebGL 2.0, WASM |
| **Firefox 111+** | ✅ Supported — directory-upload fallback for file import |
| **Safari** | ❌ Not supported — missing required directory import APIs |

### What works today

- Complete Morrowind, Tribunal, and Bloodmoon main quest lines
- WebGL 2.0 rendering with automatic GLSL ES 3.00 shader transformation
- Persistent saves and config via IndexedDB (survives browser reloads)
- Full audio via Web Audio API
- Keyboard and mouse input with pointer lock
- Tribunal and Bloodmoon expansion auto-detection
- Single-threaded and multi-threaded (pthread/Web Worker) modes
- Custom HTML shell with loading progress, data picker, and console overlay
- In-game HUD toolbar with FPS counter, copy-log, and bug-report links

### What's in progress

- End-to-end browser testing with real Morrowind data
- Performance profiling and optimization toward 30+ fps in all areas
  (see [PERFORMANCE_ROADMAP.md](PERFORMANCE_ROADMAP.md))
- Mod compatibility validation through the browser file picker

See [WASM_ROADMAP.md](WASM_ROADMAP.md) for the full technical status and
[USER_TESTING.md](USER_TESTING.md) for the structured testing guide.

---

Building for the Browser
-------------------------

Configure CMake with an Emscripten toolchain:

```bash
# Full build (pthread support, requires COOP/COEP server headers)
cmake -DOPENMW_EXPERIMENTAL_WASM=ON ...

# Single-threaded build (works with any HTTP server)
cmake -DOPENMW_EXPERIMENTAL_WASM=ON -DOPENMW_EXPERIMENTAL_WASM_PTHREADS=OFF ...
```

The full dependency build and CMake configuration is automated by
`CI/before_script.wasm.sh`. The CI job (`Emscripten_WASM` in
`.gitlab-ci.yml`) produces the `openmw.html`, `openmw.js`, and `openmw.wasm`
artifacts.

### Serving locally

```bash
# Non-pthread build — any HTTP server works
python3 -m http.server 8080 --directory /path/to/build/output

# Pthread build — requires cross-origin isolation headers
python3 scripts/serve_wasm.py /path/to/build/output --port 8080
```

### Persistent storage

Saves and config live at `/persistent` (IDBFS-backed). The mount root can be
overridden with `OPENMW_WASM_PERSISTENT_ROOT`. Background syncs run every
15 seconds and on tab/page-hide events; the interval is tunable via
`OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS`.

---

Desktop Build
-------------

OpenMW also runs natively on Windows, Linux, macOS, and Android. The desktop
build includes the full engine plus OpenMW-CS (a replacement for Bethesda's
Construction Set).

### Getting started (desktop)

* [Installation instructions](https://openmw.readthedocs.io/en/latest/manuals/installation/index.html)
* [Build from source](https://wiki.openmw.org/index.php?title=Development_Environment_Setup)
* [Testing the game](https://wiki.openmw.org/index.php?title=Testing)

### The data path

The data path tells OpenMW where to find your Morrowind files. If you run the
launcher, OpenMW should be able to pick up the location of these files on its
own, if both Morrowind and OpenMW are installed properly (installing Morrowind
under WINE is considered a proper install).

### Command line options

    Syntax: openmw <options>
    Allowed options:
      --config arg                          additional config directories
      --replace arg                         settings where the values from the
                                            current source should replace those
                                            from lower-priority sources instead of
                                            being appended
      --user-data arg                       set user data directory (used for
                                            saves, screenshots, etc)
      --resources arg (=resources)          set resources directory
      --help                                print help message
      --version                             print version information and quit
      --data arg (=data)                    set data directories (later directories
                                            have higher priority)
      --data-local arg                      set local data directory (highest
                                            priority)
      --fallback-archive arg (=fallback-archive)
                                            set fallback BSA archives (later
                                            archives have higher priority)
      --start arg                           set initial cell
      --content arg                         content file(s): esm/esp, or
                                            omwgame/omwaddon/omwscripts
      --groundcover arg                     groundcover content file(s): esm/esp,
                                            or omwgame/omwaddon
      --no-sound [=arg(=1)] (=0)            disable all sounds
      --script-all [=arg(=1)] (=0)          compile all scripts (excluding dialogue
                                            scripts) at startup
      --script-all-dialogue [=arg(=1)] (=0) compile all dialogue scripts at startup
      --script-console [=arg(=1)] (=0)      enable console-only script
                                            functionality
      --script-run arg                      select a file containing a list of
                                            console commands that is executed on
                                            startup
      --script-warn [=arg(=1)] (=1)         handling of warnings when compiling
                                            scripts
                                            0 - ignore warnings
                                            1 - show warnings but consider script as
                                            correctly compiled anyway
                                            2 - treat warnings as errors
      --load-savegame arg                   load a save game file on game startup
                                            (specify an absolute filename or a
                                            filename relative to the current
                                            working directory)
      --skip-menu [=arg(=1)] (=0)           skip main menu on game startup
      --new-game [=arg(=1)] (=0)            run new game sequence (ignored if
                                            skip-menu=0)
      --encoding arg (=win1252)             Character encoding used in OpenMW game
                                            messages:

                                            win1250 - Central and Eastern European
                                            such as Polish, Czech, Slovak,
                                            Hungarian, Slovene, Bosnian, Croatian,
                                            Serbian (Latin script), Romanian and
                                            Albanian languages

                                            win1251 - Cyrillic alphabet such as
                                            Russian, Bulgarian, Serbian Cyrillic
                                            and other languages

                                            win1252 - Western European (Latin)
                                            alphabet, used by default
      --fallback arg                        fallback values
      --no-grab [=arg(=1)] (=0)             Don't grab mouse cursor
      --export-fonts [=arg(=1)] (=0)        Export Morrowind .fnt fonts to PNG
                                            image and XML file in current directory
      --activate-dist arg (=-1)             activation distance override
      --random-seed arg (=<impl defined>)   seed value for random number generator

---

Current Status
--------------

The main quests in Morrowind, Tribunal and Bloodmoon are all completable. Some
issues with side quests are to be expected (but rare). Check the
[bug tracker](https://gitlab.com/OpenMW/openmw/-/issues/?milestone_title=openmw-1.0)
for a list of issues we need to resolve before the "1.0" release. Even before
the "1.0" release, OpenMW boasts new
[features](https://wiki.openmw.org/index.php?title=Features), such as improved
graphics and user interfaces.

Pre-existing modifications created for the original Morrowind engine can be
hit-and-miss. The OpenMW script compiler performs more thorough error-checking
than Morrowind does, meaning that a mod created for Morrowind may not
necessarily run in OpenMW. Some mods also rely on quirky behaviour or engine
bugs in order to work. We are considering such compatibility issues on a
case-by-case basis — in some cases adding a workaround to OpenMW may be
feasible, in other cases fixing the mod will be the only option. If you know of
any mods that work or don't work, feel free to add them to the
[Mod status](https://wiki.openmw.org/index.php?title=Mod_status) wiki page.

---

Contributing
------------

* [How to contribute](https://wiki.openmw.org/index.php?title=Contribution_Wanted)
* [Report a bug](https://gitlab.com/OpenMW/openmw/issues) — read the [guidelines](https://wiki.openmw.org/index.php?title=Bug_Reporting_Guidelines) before submitting your first bug!
* [Known issues](https://gitlab.com/OpenMW/openmw/issues?label_name%5B%5D=Bug)
* [Official forums](https://forum.openmw.org/)
* [Performance roadmap](PERFORMANCE_ROADMAP.md)
* [WASM technical roadmap](WASM_ROADMAP.md)
* [Browser testing guide](USER_TESTING.md)

---

Font Licenses
-------------

* DejaVuLGCSansMono.ttf: custom (see [DejaVuFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/DejaVuFontLicense.txt))
* DemonicLetters.ttf: SIL Open Font License (see [DemonicLettersFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/DemonicLettersFontLicense.txt))
* MysticCards.ttf: SIL Open Font License (see [MysticCardsFontLicense.txt](https://gitlab.com/OpenMW/openmw/-/raw/master/files/data/fonts/MysticCardsFontLicense.txt))
