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


if __name__ == "__main__":
    unittest.main()
