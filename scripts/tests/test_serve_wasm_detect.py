import tempfile
import unittest
from pathlib import Path

import sys

# Import from scripts/ (same pattern as sibling tests reaching into repo root).
_SCRIPTS = Path(__file__).resolve().parents[1]
if str(_SCRIPTS) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS))

from serve_wasm_detect import detect_pthread_openmw_js


class ServeWasmDetectTests(unittest.TestCase):
    def test_missing_js_returns_none(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertIsNone(detect_pthread_openmw_js(d))

    def test_empty_js_returns_none(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, "openmw.js").write_text("", encoding="utf-8")
            self.assertIsNone(detect_pthread_openmw_js(d))

    def test_plain_stub_returns_false(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, "openmw.js").write_text(
                "var Module = { canvas: document.getElementById('canvas') };\n",
                encoding="utf-8",
            )
            self.assertIs(detect_pthread_openmw_js(d), False)

    def test_pthread_marker_returns_true(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, "openmw.js").write_text(
                "// Emscripten runtime\nvar PThread = {};\n",
                encoding="utf-8",
            )
            self.assertIs(detect_pthread_openmw_js(d), True)


if __name__ == "__main__":
    unittest.main()
