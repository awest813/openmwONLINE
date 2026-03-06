import re
import unittest
from pathlib import Path


class WasmShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        repo_root = Path(__file__).resolve().parents[2]
        cls.shell_html = (repo_root / "files" / "wasm" / "openmw_shell.html").read_text(encoding="utf-8")
        cls.wasm_picker_cpp = (repo_root / "apps" / "openmw" / "wasmfilepicker.cpp").read_text(encoding="utf-8")

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

    def test_periodic_idbfs_sync_is_configured(self):
        self.assertIn("function startPeriodicSync()", self.shell_html)
        self.assertIn("function stopPeriodicSync()", self.shell_html)
        self.assertIn("SYNC_INTERVAL_MS", self.shell_html)

    def test_page_lifecycle_events_trigger_sync(self):
        self.assertIn("visibilitychange", self.shell_html)
        self.assertIn("pagehide", self.shell_html)
        self.assertIn("beforeunload", self.shell_html)
        self.assertRegex(
            self.shell_html,
            r"addEventListener\(\s*'visibilitychange'",
            msg="Expected visibilitychange event listener for IDBFS sync",
        )

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

    def test_runtime_init_starts_periodic_sync_and_heap_monitor(self):
        self.assertRegex(
            self.shell_html,
            r"onRuntimeInitialized:[\s\S]*?startPeriodicSync\(\)",
            msg="Expected onRuntimeInitialized to call startPeriodicSync()",
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


if __name__ == "__main__":
    unittest.main()
