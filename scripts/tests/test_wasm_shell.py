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
        cls.statemanager_cpp = (repo_root / "apps" / "openmw" / "mwstate" / "statemanagerimp.cpp").read_text(encoding="utf-8")

    def test_successful_data_upload_reveals_canvas(self):
        self.assertIn("function showCanvas()", self.shell_html)
        self.assertRegex(
            self.shell_html,
            r"if\s*\(success\)\s*\{[\s\S]*?showCanvas\(\);[\s\S]*?return;",
            msg="Expected success branch to reveal canvas and return",
        )

    def test_unsuccessful_upload_resets_button_and_handles_status_codes(self):
        self.assertIn("resetDataPickerButton(btn);", self.shell_html)
        self.assertIn("Step 1 — Select Morrowind Data Files folder", self.shell_html)
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


    # ------------------------------------------------------------------
    # Remaining crash/stall vector fixes (this PR)
    # ------------------------------------------------------------------

    def test_audio_context_auto_resume(self):
        """resumeAudioContext() must exist and be called from both the
        click-to-play handler (user gesture satisfies the browser's autoplay
        policy) and the visibilitychange handler (resumes the AudioContext
        suspended by the browser when the tab was in the background), so that
        in-game audio is never permanently silenced during a long playthrough."""
        self.assertIn(
            "function resumeAudioContext()",
            self.shell_html,
            msg="Expected resumeAudioContext() function in shell HTML",
        )
        # Must check and resume via AL.currentCtx.ctx (Emscripten OpenAL path)
        self.assertIn(
            "AL.currentCtx.ctx",
            self.shell_html,
            msg="Expected AL.currentCtx.ctx reference in resumeAudioContext()",
        )
        self.assertIn(
            "ctx.resume()",
            self.shell_html,
            msg="Expected ctx.resume() call in resumeAudioContext()",
        )
        # Must be called from the click-to-play handler (after requestPointerLock)
        self.assertRegex(
            self.shell_html,
            r"requestPointerLock[\s\S]{0,300}resumeAudioContext\(\)",
            msg="Expected resumeAudioContext() to be called in the click-to-play click handler",
        )
        # Must be called from the visible branch of visibilitychange (i.e. the
        # else block that runs when the tab becomes visible, not the hidden branch)
        self.assertRegex(
            self.shell_html,
            r"visibilityState === 'hidden'[\s\S]{0,200}}\s*else\s*\{[\s\S]{0,300}resumeAudioContext\(\)",
            msg="Expected resumeAudioContext() to be called in the else (visible) branch of visibilitychange",
        )

    def test_storage_quota_exceeded_warning(self):
        """When IDBFS sync fails with a QuotaExceededError the shell must
        surface a visible, actionable warning to the player — revealing the
        HUD toolbar — rather than silently dropping saves and logging only to
        the developer console."""
        self.assertIn(
            "function showStorageQuotaWarning()",
            self.shell_html,
            msg="Expected showStorageQuotaWarning() function in shell HTML",
        )
        # Quota detection must check for QuotaExceededError by name
        self.assertIn(
            "QuotaExceededError",
            self.shell_html,
            msg="Expected QuotaExceededError name check in IDBFS sync error handler",
        )
        # QuotaExceededError detection must be inside the sync error callback
        self.assertRegex(
            self.shell_html,
            r"IDBFS sync failed[\s\S]{0,500}QuotaExceededError",
            msg="Expected QuotaExceededError detection within the IDBFS sync failed error handler",
        )
        # showStorageQuotaWarning must be called when quota error is detected
        self.assertRegex(
            self.shell_html,
            r"QuotaExceededError[\s\S]{0,300}showStorageQuotaWarning\(\)",
            msg="Expected showStorageQuotaWarning() call when QuotaExceededError is detected",
        )
        # Warning function must reveal the HUD toolbar
        self.assertRegex(
            self.shell_html,
            r"function showStorageQuotaWarning\(\)[\s\S]{0,600}hudToolbar\.classList\.add\('visible'\)",
            msg="Expected showStorageQuotaWarning() to reveal the HUD toolbar",
        )

    def test_wasm_save_duration_warning(self):
        """In WASM builds saveGame() must log a Warning when the write takes
        more than 2 s so that late-game save stalls are visible in bug reports.
        Large save files can block the main thread long enough to trigger the
        browser's 'Page Unresponsive' dialog."""
        self.assertIn(
            "__EMSCRIPTEN__",
            self.statemanager_cpp,
            msg="Expected __EMSCRIPTEN__ guard in statemanagerimp.cpp",
        )
        self.assertIn(
            "saveMs",
            self.statemanager_cpp,
            msg="Expected saveMs duration variable in statemanagerimp.cpp WASM block",
        )
        self.assertRegex(
            self.statemanager_cpp,
            r"saveMs[\s\S]{0,300}Page Unresponsive",
            msg="Expected WASM save-duration warning mentioning 'Page Unresponsive' in statemanagerimp.cpp",
        )
        # The warning must be inside the EMSCRIPTEN block that also calls the
        # IDBFS sync, to keep all WASM-specific post-save logic together.
        self.assertRegex(
            self.statemanager_cpp,
            r"saveMs[\s\S]{0,600}__openmwSyncPersistentStorage",
            msg="Expected saveMs warning and IDBFS sync to be in the same __EMSCRIPTEN__ block",
        )

    # ------------------------------------------------------------------
    # Debug / audit / polish for live user testing
    # ------------------------------------------------------------------

    def test_fps_counter_raf_patching(self):
        """The shell must install an FPS counter by patching
        window.requestAnimationFrame so every frame is counted without
        requiring Emscripten to expose an explicit frame hook.  The counter
        must reference the hud-fps element so testers can read it."""
        self.assertIn(
            "function recordFrame()",
            self.shell_html,
            msg="Expected recordFrame() function in shell HTML",
        )
        self.assertIn(
            "fpsSamples",
            self.shell_html,
            msg="Expected fpsSamples array for the sliding 1-second FPS window",
        )
        # RAF must be patched to call recordFrame on every animation frame
        self.assertRegex(
            self.shell_html,
            r"window\.requestAnimationFrame\s*=\s*function",
            msg="Expected window.requestAnimationFrame to be replaced with a wrapper",
        )
        # The wrapper must call the original RAF and invoke recordFrame
        self.assertRegex(
            self.shell_html,
            r"_origRAF[\s\S]{0,300}recordFrame\(\)",
            msg="Expected the RAF wrapper to invoke recordFrame() on each frame",
        )
        # The FPS counter must write to the hud-fps element
        self.assertRegex(
            self.shell_html,
            r"hudFps\.textContent\s*=",
            msg="Expected recordFrame() to update the hudFps element textContent",
        )

    def test_report_phase_globally_exposed(self):
        """reportPhase() must be assigned to globalThis.__openmwReportPhase so
        that the browser smoke-test harness and any external test scripts can
        inject phase events and read the phase timeline without importing
        internal shell state."""
        self.assertIn(
            "globalThis.__openmwReportPhase = reportPhase",
            self.shell_html,
            msg="Expected globalThis.__openmwReportPhase = reportPhase in shell HTML",
        )

    def test_phase_timeline_globally_exposed(self):
        """Every reportPhase() call must update globalThis.__openmwPhaseTimeline
        with a snapshot of the current timeline so that automated test harnesses
        and browser devtools can inspect the sequence of startup and gameplay
        phases without subscribing to the openmw:phase CustomEvent."""
        self.assertIn(
            "globalThis.__openmwPhaseTimeline = phaseTimeline.slice()",
            self.shell_html,
            msg="Expected globalThis.__openmwPhaseTimeline to be updated in reportPhase()",
        )
        # The update must happen inside reportPhase() before the CustomEvent dispatch
        self.assertRegex(
            self.shell_html,
            r"function reportPhase\([\s\S]{0,400}__openmwPhaseTimeline\s*=\s*phaseTimeline\.slice\(\)",
            msg="Expected __openmwPhaseTimeline update inside the reportPhase() body",
        )

    def test_pointer_lock_error_handler(self):
        """A pointerlockerror handler must be registered so that when the
        browser refuses pointer-lock (e.g. missing user gesture or sandboxed
        iframe) the shell logs the failure and re-shows the click-to-play
        overlay rather than leaving the game in a broken input state."""
        self.assertIn(
            "pointerlockerror",
            self.shell_html,
            msg="Expected pointerlockerror event handler in shell HTML",
        )
        # The handler must log the failure for tester bug reports
        self.assertRegex(
            self.shell_html,
            r"pointerlockerror[\s\S]{0,400}logToConsole",
            msg="Expected pointerlockerror handler to call logToConsole()",
        )
        # The handler must restore the click-to-play overlay so the player can retry
        self.assertRegex(
            self.shell_html,
            r"pointerlockerror[\s\S]{0,400}classList\.remove\('hidden'\)",
            msg="Expected pointerlockerror handler to re-show the click-to-play overlay",
        )

    def test_hud_toolbar_pinned_by_warnings(self):
        """When a critical memory warning or storage-quota warning makes the
        HUD toolbar visible, it must stay visible even when the player
        subsequently presses Ctrl+` to toggle the console overlay.  Without
        the _hudPinned guard, toggleConsole() would silently hide the HUD
        and the player would lose sight of the warning mid-session."""
        self.assertIn(
            "var _hudPinned = false",
            self.shell_html,
            msg="Expected _hudPinned flag variable in shell HTML",
        )
        # toggleConsole must check _hudPinned before hiding the HUD
        self.assertRegex(
            self.shell_html,
            r"function toggleConsole\(\)[\s\S]{0,400}_hudPinned",
            msg="Expected toggleConsole() to check _hudPinned before changing HUD visibility",
        )
        # showStorageQuotaWarning must set _hudPinned = true
        self.assertRegex(
            self.shell_html,
            r"function showStorageQuotaWarning\(\)[\s\S]{0,600}_hudPinned\s*=\s*true",
            msg="Expected showStorageQuotaWarning() to set _hudPinned = true",
        )
        # Critical memory branch must also set _hudPinned = true
        self.assertRegex(
            self.shell_html,
            r"HEAP_CRITICAL_THRESHOLD[\s\S]{0,700}_hudPinned\s*=\s*true",
            msg="Expected critical memory branch to set _hudPinned = true",
        )

    def test_error_panel_includes_phase_timeline(self):
        """showError() must embed the phase timeline in the auto-generated
        GitLab issue URL so that bug reports filed by live testers include
        structured context about which startup and gameplay phases completed
        before the crash — making it far easier to reproduce and triage.
        The number of entries to include must be controlled by a named constant
        (MAX_PHASE_TIMELINE_REPORT_ENTRIES) rather than a bare magic number."""
        # A named constant must govern the number of entries in the report
        self.assertIn(
            "MAX_PHASE_TIMELINE_REPORT_ENTRIES",
            self.shell_html,
            msg="Expected MAX_PHASE_TIMELINE_REPORT_ENTRIES constant in shell HTML",
        )
        # showError() must reference phaseTimeline when building the issue body
        self.assertRegex(
            self.shell_html,
            r"function showError\([\s\S]{0,800}phaseTimeline",
            msg="Expected showError() to include phaseTimeline in the error report body",
        )
        # showError() must use the named constant
        self.assertRegex(
            self.shell_html,
            r"function showError\([\s\S]{0,800}MAX_PHASE_TIMELINE_REPORT_ENTRIES",
            msg="Expected showError() to slice phaseTimeline using MAX_PHASE_TIMELINE_REPORT_ENTRIES",
        )
        # The timeline section heading must appear in the report template
        self.assertIn(
            "Phase timeline",
            self.shell_html,
            msg="Expected 'Phase timeline' section heading in the error report template",
        )

    def test_status_element_has_aria_live(self):
        """The #status element must carry role='status' and aria-live='polite'
        so that assistive technologies announce status updates (engine ready,
        data loaded, errors) without requiring the user to focus the element."""
        self.assertIn(
            'aria-live="polite"',
            self.shell_html,
            msg="Expected aria-live=\"polite\" on the #status element for accessibility",
        )
        self.assertIn(
            'role="status"',
            self.shell_html,
            msg="Expected role=\"status\" on the #status element for semantic markup",
        )
        # Both attributes must be on the same element as id="status"
        self.assertRegex(
            self.shell_html,
            r'id="status"[^>]*aria-live="polite"|aria-live="polite"[^>]*id="status"',
            msg="Expected aria-live=\"polite\" to be on the element with id=\"status\"",
        )
        self.assertRegex(
            self.shell_html,
            r'id="status"[^>]*role="status"|role="status"[^>]*id="status"',
            msg="Expected role=\"status\" to be on the element with id=\"status\"",
        )

    def test_wasm_performance_defaults_cover_all_settings(self):
        """The WASM build must override all heavyweight default settings so that
        first-time testers start with a playable frame rate rather than having to
        manually tune settings.cfg.  Required overrides:
          - shadows disabled (most expensive single setting)
          - viewing distance capped at 4096 (limits scene complexity)
          - MSAA disabled (WebGL MSAA is slow on mobile GPUs)
          - reverse-Z disabled (WebGL extension rarely available)
          - small-feature culling pixel size >= 4.0
          - cache expiry delay capped at 2.0 s
          - object paging disabled (saves draw calls & VRAM)
          - composite-map resolution capped at 256
          - frame-rate limit capped at 60 fps
        """
        # All overrides must be inside an __EMSCRIPTEN__ guard
        self.assertIn(
            "__EMSCRIPTEN__",
            self.engine_cpp,
            msg="Expected __EMSCRIPTEN__ guard in engine.cpp",
        )
        for symbol in (
            "mEnableShadows",
            "mViewingDistance",
            "mAntialiasing",
            "mReverseZ",
            "mSmallFeatureCullingPixelSize",
            "mCacheExpiryDelay",
            "mObjectPaging",
            "mCompositeMapResolution",
            "mFramerateLimit",
        ):
            self.assertIn(
                symbol,
                self.engine_cpp,
                msg=f"Expected WASM performance default for {symbol} in engine.cpp",
            )

    def test_issue_body_uses_named_log_lines_constant(self):
        """showError() must use a named constant (MAX_ISSUE_BODY_LOG_LINES) when
        slicing logLines for the auto-generated GitLab issue URL so that the
        number of included log lines is self-documenting and easy to adjust
        without introducing a hard-to-spot magic number."""
        self.assertIn(
            "MAX_ISSUE_BODY_LOG_LINES",
            self.shell_html,
            msg="Expected MAX_ISSUE_BODY_LOG_LINES constant in shell HTML",
        )
        # showError() must use the named constant (not a bare literal) when slicing
        self.assertRegex(
            self.shell_html,
            r"function showError\([\s\S]{0,1000}logLines\.slice\(-MAX_ISSUE_BODY_LOG_LINES\)",
            msg="Expected showError() to use MAX_ISSUE_BODY_LOG_LINES when slicing logLines",
        )
        # The constant must be declared before showError() uses it
        constant_pos = self.shell_html.find("MAX_ISSUE_BODY_LOG_LINES =")
        show_error_pos = self.shell_html.find("function showError(")
        self.assertNotEqual(constant_pos, -1, "Expected MAX_ISSUE_BODY_LOG_LINES assignment in shell HTML")
        self.assertNotEqual(show_error_pos, -1, "Expected showError() function in shell HTML")
        self.assertLess(
            constant_pos,
            show_error_pos,
            msg="MAX_ISSUE_BODY_LOG_LINES must be declared before showError()",
        )

    def test_webgl_capabilities_logged_at_runtime_init(self):
        """onRuntimeInitialized must probe and log WebGL extension availability
        (EXT_color_buffer_float, EXT_color_buffer_half_float) so that live
        testers and automated smoke tests can correlate visual quality with the
        browser's actual WebGL extension support (USER_TESTING.md Phase 3.5).
        The probe result must also be reported as a phase entry so it is included
        in auto-generated GitLab issue timelines."""
        # Both HDR-relevant extensions must be probed
        self.assertRegex(
            self.shell_html,
            r"onRuntimeInitialized:[\s\S]*?EXT_color_buffer_float",
            msg="Expected EXT_color_buffer_float capability probe in onRuntimeInitialized",
        )
        self.assertRegex(
            self.shell_html,
            r"onRuntimeInitialized:[\s\S]*?EXT_color_buffer_half_float",
            msg="Expected EXT_color_buffer_half_float capability probe in onRuntimeInitialized",
        )
        # The probe result must be logged via logToConsole for the in-game overlay
        self.assertRegex(
            self.shell_html,
            r"EXT_color_buffer_float[\s\S]{0,400}logToConsole",
            msg="Expected WebGL capability probe result to be logged via logToConsole()",
        )
        # The probe must emit a reportPhase('webgl_capabilities', ...) entry so the
        # capability state is captured in the phase timeline and error reports.
        self.assertRegex(
            self.shell_html,
            r"reportPhase\(['\"]webgl_capabilities['\"]",
            msg="Expected reportPhase('webgl_capabilities', ...) call in WebGL capability probe",
        )

    def test_file_picker_path_traversal_prevention(self):
        """The WASM file picker must strip path-traversal components ('..') from
        every uploaded file path via normalizeRelativePath() so that a malicious
        or malformed data archive cannot write files outside the designated data
        mount directory."""
        self.assertIn(
            "normalizeRelativePath",
            self.wasm_picker_cpp,
            msg="Expected normalizeRelativePath() function in wasmfilepicker.cpp",
        )
        # The function body must explicitly skip '..' path components using a
        # continue statement inside a conditional (not merely mention the string).
        self.assertRegex(
            self.wasm_picker_cpp,
            r"part === '\.\.'[\s\S]{0,50}continue",
            msg="Expected normalizeRelativePath() to skip '..' components with 'continue'",
        )
        # normalizeRelativePath must be called and the sanitised result must be
        # the path actually used when writing to the VFS — not the raw input.
        # Both __openmwUploadFile and uploadFileChunked must use the pattern
        # 'normalizedPath = normalizeRelativePath(...)' before touching mountPath.
        self.assertRegex(
            self.wasm_picker_cpp,
            r"normalizedPath\s*=\s*normalizeRelativePath\(",
            msg="Expected 'normalizedPath = normalizeRelativePath(...)' pattern in file-writing code",
        )
        # The VFS write path must be constructed from the sanitised normalizedPath,
        # not from the raw relativePath argument.
        self.assertRegex(
            self.wasm_picker_cpp,
            r"mountPath\s*\+\s*['\"/]\/['\"/]\s*\+\s*normalizedPath",
            msg="Expected VFS write path to be built from normalizedPath (not raw relativePath)",
        )


if __name__ == "__main__":
    unittest.main()
