import re
import unittest
from pathlib import Path


class WasmShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        repo_root = Path(__file__).resolve().parents[2]
        cls.shell_html = (repo_root / "files" / "wasm" / "openmw_shell.html").read_text(encoding="utf-8")
        cls.wasm_picker_cpp = (repo_root / "apps" / "openmw" / "wasmfilepicker.cpp").read_text(encoding="utf-8")
        cls.engine_cpp = (repo_root / "apps" / "openmw" / "engine.cpp").read_text(encoding="utf-8")
        cls.loadingscreen_cpp = (repo_root / "apps" / "openmw" / "mwgui" / "loadingscreen.cpp").read_text(encoding="utf-8")

    def test_successful_data_upload_reveals_canvas(self):
        self.assertIn("function showCanvas()", self.shell_html)
        self.assertRegex(
            self.shell_html,
            r"if\s*\(success\)\s*\{[\s\S]*?showCanvas\(\);[\s\S]*?return;",
            msg="Expected success branch to reveal canvas and return",
        )

    def test_unsuccessful_upload_resets_button_and_handles_status_codes(self):
        self.assertIn("resetDataPickerButton(btn);", self.shell_html)
        self.assertIn("pickStatus === 'cancelled'", self.shell_html)
        self.assertIn("pickStatus === 'validation-failed'", self.shell_html)
        self.assertIn("pickStatus === 'empty-selection'", self.shell_html)
        self.assertIn("pickStatus === 'unsupported'", self.shell_html)
        self.assertIn("__openmwGetLastPickResult", self.shell_html)

    def test_wasm_picker_bridge_supports_fallback_cleanup_and_validation(self):
        self.assertIn("function getPickerCapabilities()", self.wasm_picker_cpp)
        self.assertIn("pickFilesViaInput", self.wasm_picker_cpp)
        self.assertIn("webkitdirectory", self.wasm_picker_cpp)
        self.assertIn("clearDataMountDirectory()", self.wasm_picker_cpp)
        self.assertIn("hasRequiredBaseFiles", self.wasm_picker_cpp)
        self.assertIn("morrowind.esm", self.wasm_picker_cpp)

    def test_idbfs_sync_coalescing_function_exists(self):
        self.assertIn("function syncPersistentStorage(reason)", self.shell_html)
        self.assertIn("_idbfsSyncInFlight", self.shell_html)
        self.assertIn("_idbfsSyncQueued", self.shell_html)
        self.assertIn("__openmwSyncPersistentStorage", self.shell_html)

    def test_idbfs_sync_timeout_mechanism_is_present(self):
        self.assertIn("SYNC_TIMEOUT_MS", self.shell_html)
        self.assertIn("IDBFS sync timed out", self.shell_html)
        self.assertIn("queued-after-timeout", self.shell_html)
        self.assertIn("clearTimeout(syncTimeoutHandle)", self.shell_html)

    def test_periodic_idbfs_sync_is_configured(self):
        self.assertIn("function startPeriodicSync()", self.shell_html)
        self.assertIn("function stopPeriodicSync()", self.shell_html)
        self.assertIn("SYNC_INTERVAL_MS", self.shell_html)

    def test_page_lifecycle_events_trigger_sync(self):
        self.assertIn("visibilitychange", self.shell_html)
        self.assertIn("pagehide", self.shell_html)
        self.assertIn("beforeunload", self.shell_html)
        self.assertIn("freeze", self.shell_html)
        self.assertRegex(
            self.shell_html,
            r"addEventListener\(\s*'visibilitychange'",
            msg="Expected visibilitychange event listener for IDBFS sync",
        )

    def test_persistent_storage_request_is_attempted(self):
        self.assertIn("function ensurePersistentStorage()", self.shell_html)
        self.assertIn("navigator.storage.persisted", self.shell_html)
        self.assertIn("navigator.storage.persist()", self.shell_html)
        self.assertIn("persistence_storage", self.shell_html)

    def test_webgl_context_loss_handling(self):
        self.assertIn("webglcontextlost", self.shell_html)
        self.assertIn("webglcontextrestored", self.shell_html)
        self.assertIn("_webglContextLost", self.shell_html)

    def test_heap_memory_monitoring(self):
        self.assertIn("function checkHeapUsage()", self.shell_html)
        self.assertIn("HEAP_WARNING_THRESHOLD", self.shell_html)
        self.assertIn("HEAP_WARNING_HYSTERESIS", self.shell_html)
        self.assertIn("WASM_MAX_HEAP_BYTES", self.shell_html)
        self.assertIn("function startHeapMonitor()", self.shell_html)

    def test_heap_critical_threshold_triggers_sync(self):
        """At the critical memory threshold (>92% heap) the shell must flush
        saves to IndexedDB immediately and reveal the HUD toolbar so the
        user is aware of the risk of OOM tab termination."""
        self.assertIn("HEAP_CRITICAL_THRESHOLD", self.shell_html)
        self.assertIn("_heapCriticalShown", self.shell_html)
        # Critical branch must call syncPersistentStorage
        self.assertRegex(
            self.shell_html,
            r"HEAP_CRITICAL_THRESHOLD[\s\S]{0,600}syncPersistentStorage\(",
            msg="Expected critical memory branch to call syncPersistentStorage()",
        )
        # Critical branch must reveal the HUD toolbar
        self.assertRegex(
            self.shell_html,
            r"HEAP_CRITICAL_THRESHOLD[\s\S]{0,600}hudToolbar\.classList\.add\('visible'\)",
            msg="Expected critical memory branch to show the HUD toolbar",
        )

    def test_hud_toolbar_has_memory_display_element(self):
        """The HUD toolbar must contain a dedicated element for showing live
        heap-memory usage so testers can monitor memory pressure at a glance."""
        self.assertIn('id="hud-mem"', self.shell_html)
        self.assertRegex(
            self.shell_html,
            r"getElementById\(['\"]hud-mem['\"]",
            msg="Expected checkHeapUsage() to look up the hud-mem element",
        )

    def test_canvas_resize_observer_is_installed(self):
        """A ResizeObserver (with a window-resize fallback) must be set up on
        the canvas so that the canvas pixel dimensions stay in sync with the
        CSS display size when the browser window is resized, preventing
        stretched or blurry rendering."""
        self.assertIn("syncCanvasSize", self.shell_html)
        self.assertIn("ResizeObserver", self.shell_html)
        self.assertIn("window.addEventListener('resize', syncCanvasSize)", self.shell_html)
        # The observer must target the canvas element
        self.assertRegex(
            self.shell_html,
            r"new ResizeObserver\(syncCanvasSize\)\.observe\(canvas\)",
            msg="Expected ResizeObserver to observe the canvas element",
        )

    def test_runtime_init_starts_periodic_sync_and_heap_monitor(self):
        self.assertRegex(
            self.shell_html,
            r"onRuntimeInitialized:[\s\S]*?startPeriodicSync\(\)",
            msg="Expected onRuntimeInitialized to call startPeriodicSync()",
        )
        self.assertRegex(
            self.shell_html,
            r"onRuntimeInitialized:[\s\S]*?ensurePersistentStorage\(\)",
            msg="Expected onRuntimeInitialized to call ensurePersistentStorage()",
        )
        self.assertRegex(
            self.shell_html,
            r"onRuntimeInitialized:[\s\S]*?startHeapMonitor\(\)",
            msg="Expected onRuntimeInitialized to call startHeapMonitor()",
        )

    def test_abort_handler_syncs_and_stops_periodic(self):
        self.assertRegex(
            self.shell_html,
            r"onAbort:[\s\S]*?stopPeriodicSync\(\)",
            msg="Expected onAbort to call stopPeriodicSync()",
        )
        self.assertRegex(
            self.shell_html,
            r"onAbort:[\s\S]*?syncPersistentStorage\(",
            msg="Expected onAbort to trigger a final sync",
        )

    def test_shell_sets_persistent_sync_registered_flag(self):
        """The HTML shell must set window.__openmwPersistentSyncRegistered = true after
        registering lifecycle event listeners so that the C++ persistence bootstrap
        script (injected by initializeWasmPersistentStorage via emscripten_run_script)
        does not add a second set of duplicate sync handlers.  Duplicate handlers can
        trigger simultaneous FS.syncfs() calls which corrupt IndexedDB save data."""
        self.assertIn(
            "window.__openmwPersistentSyncRegistered = true",
            self.shell_html,
            msg="Expected shell to set window.__openmwPersistentSyncRegistered = true "
                "after registering lifecycle event listeners",
        )
        # The flag must be set AFTER the freeze listener (the last lifecycle listener)
        # and BEFORE the WebGL context-loss block so it takes effect before main() runs.
        freeze_pos = self.shell_html.find("addEventListener('freeze'")
        flag_pos = self.shell_html.find("window.__openmwPersistentSyncRegistered = true")
        webgl_pos = self.shell_html.find("webglcontextlost")
        self.assertNotEqual(freeze_pos, -1, "Expected freeze event listener in shell")
        self.assertNotEqual(flag_pos, -1, "Expected __openmwPersistentSyncRegistered flag assignment in shell")
        self.assertNotEqual(webgl_pos, -1, "Expected webglcontextlost handler in shell")
        self.assertLess(
            freeze_pos,
            flag_pos,
            msg="__openmwPersistentSyncRegistered must be set after the freeze listener",
        )
        self.assertLess(
            flag_pos,
            webgl_pos,
            msg="__openmwPersistentSyncRegistered must be set before the WebGL context-loss block",
        )

    def test_cpp_persistence_script_guards_against_duplicate_sync(self):
        """The C++ persistence bootstrap script (in main.cpp) must check
        _shellAlreadyRegistered before overwriting __openmwSyncPersistentStorage and
        before starting the periodic sync timer, so that the HTML shell's superior
        coalesced implementation is preserved when running with the standard shell."""
        repo_root = Path(__file__).resolve().parents[2]
        main_cpp = (repo_root / "apps" / "openmw" / "main.cpp").read_text(encoding="utf-8")

        self.assertIn(
            "_shellAlreadyRegistered",
            main_cpp,
            msg="Expected _shellAlreadyRegistered guard variable in main.cpp persistence script",
        )
        self.assertIn(
            "window.__openmwPersistentSyncRegistered",
            main_cpp,
            msg="Expected main.cpp persistence script to read window.__openmwPersistentSyncRegistered",
        )
        # The assignment to __openmwSyncPersistentStorage must be guarded.
        self.assertRegex(
            main_cpp,
            r"_shellAlreadyRegistered[\s\S]{0,200}__openmwSyncPersistentStorage\s*=\s*syncPersistentStorage",
            msg="Expected __openmwSyncPersistentStorage assignment to be guarded by _shellAlreadyRegistered",
        )
        # schedulePeriodicPersistentSync must be guarded too.
        self.assertRegex(
            main_cpp,
            r"_shellAlreadyRegistered[\s\S]{0,200}schedulePeriodicPersistentSync\(\)",
            msg="Expected schedulePeriodicPersistentSync() to be guarded by _shellAlreadyRegistered",
        )

    # ------------------------------------------------------------------
    # New crash/stall vector fixes
    # ------------------------------------------------------------------

    def test_prerun_idbfs_sync_has_timeout(self):
        """The preRun IDBFS sync must have a safety timeout so that a
        corrupted or unresponsive IndexedDB cannot stall the engine
        startup indefinitely.  The timeout must:
          - be declared before the FS.syncfs call,
          - call Module.removeRunDependency on expiry, and
          - be cancelled (clearTimeout) when the callback fires normally.
        """
        self.assertIn(
            "_preRunSyncTimeout",
            self.shell_html,
            msg="Expected a _preRunSyncTimeout variable in the preRun IDBFS sync block",
        )
        self.assertIn(
            "_preRunSyncSettled",
            self.shell_html,
            msg="Expected _preRunSyncSettled guard to prevent double-removal of run dependency",
        )
        # Timeout must remove the run dependency on expiry
        self.assertRegex(
            self.shell_html,
            r"_preRunSyncTimeout[\s\S]{0,600}removeRunDependency\(",
            msg="Expected _preRunSyncTimeout handler to call removeRunDependency()",
        )
        # Normal callback must cancel the timeout
        self.assertRegex(
            self.shell_html,
            r"clearTimeout\(_preRunSyncTimeout\)",
            msg="Expected FS.syncfs callback to clearTimeout(_preRunSyncTimeout)",
        )

    def test_wasm_long_frame_detection(self):
        """runWasmMainLoop must measure wall-clock time per frame and log a
        warning when a frame exceeds 2 s, so that cell-transition stalls are
        visible in the engine log and bug reports."""
        self.assertIn(
            "frameWallStart",
            self.engine_cpp,
            msg="Expected frameWallStart timing variable in runWasmMainLoop",
        )
        self.assertIn(
            "frameWallMs",
            self.engine_cpp,
            msg="Expected frameWallMs duration variable in runWasmMainLoop",
        )
        self.assertIn(
            "Long frame detected",
            self.engine_cpp,
            msg="Expected 'Long frame detected' warning message in runWasmMainLoop",
        )
        # Warning must include the measured frame time
        self.assertRegex(
            self.engine_cpp,
            r"frameWallMs[\s\S]{0,200}Long frame detected",
            msg="Expected long-frame warning to include the measured frameWallMs value",
        )

    def test_wasm_settings_flush_interval_reduced(self):
        """The WASM settings-flush interval must be at most 1 800 frames
        (~30 s at 60 fps) to reduce the data-loss window when the browser
        crashes between explicit saves (was 18 000 / ~5 min)."""
        # Extract kSettingsFlushInterval value from the C++ source.
        match = re.search(
            r"kSettingsFlushInterval\s*=\s*(\d+)",
            self.engine_cpp,
        )
        self.assertIsNotNone(
            match,
            msg="Expected kSettingsFlushInterval constant in engine.cpp",
        )
        interval = int(match.group(1))
        self.assertLessEqual(
            interval,
            1800,
            msg=f"kSettingsFlushInterval ({interval}) must be <= 1800 frames (~30 s) to limit data-loss risk",
        )

    def test_wasm_settings_flush_triggers_idbfs_sync(self):
        """After writing settings and Lua storage, runWasmMainLoop must
        trigger an IDBFS sync so the flushed data propagates to IndexedDB
        immediately (not only on the next periodic sync interval)."""
        # The flush block saves permanent storage, then immediately calls the
        # IDBFS sync.  Use savePermanentStorage as an intermediate anchor so
        # the pattern is specific to the flush block rather than matching any
        # other occurrence of __openmwSyncPersistentStorage in engine.cpp.
        self.assertRegex(
            self.engine_cpp,
            r"savePermanentStorage[\s\S]{0,300}__openmwSyncPersistentStorage",
            msg="Expected IDBFS sync trigger immediately after savePermanentStorage in runWasmMainLoop",
        )

    def test_loading_screen_triggers_idbfs_sync_on_load_start(self):
        """loadingOn() must trigger an IDBFS sync in WASM builds before the
        potentially long cell-transition or new-game load, so that any
        unsaved progress from the previous session is protected against an
        OOM tab kill or unexpected browser crash during loading."""
        self.assertIn(
            "__EMSCRIPTEN__",
            self.loadingscreen_cpp,
            msg="Expected __EMSCRIPTEN__ guard in loadingscreen.cpp",
        )
        self.assertIn(
            "emscripten.h",
            self.loadingscreen_cpp,
            msg="Expected emscripten.h include in loadingscreen.cpp",
        )
        self.assertIn(
            "__openmwSyncPersistentStorage",
            self.loadingscreen_cpp,
            msg="Expected __openmwSyncPersistentStorage call in loadingscreen.cpp loadingOn()",
        )
        # The sync call must appear inside the loadingOn function body, anchored
        # to the emscripten_run_script call that wraps it.
        self.assertRegex(
            self.loadingscreen_cpp,
            r"void LoadingScreen::loadingOn\(\)[\s\S]{0,900}emscripten_run_script[\s\S]{0,200}__openmwSyncPersistentStorage",
            msg="Expected emscripten_run_script(__openmwSyncPersistentStorage) in LoadingScreen::loadingOn()",
        )


if __name__ == "__main__":
    unittest.main()
