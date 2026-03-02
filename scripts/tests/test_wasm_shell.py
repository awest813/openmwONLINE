import re
import unittest
from pathlib import Path


class WasmShellTests(unittest.TestCase):
    def test_successful_data_upload_reveals_canvas(self):
        repo_root = Path(__file__).resolve().parents[2]
        shell_html = (repo_root / "files" / "wasm" / "openmw_shell.html").read_text(encoding="utf-8")

        self.assertIn("function showCanvas()", shell_html)

        success_flow = re.search(
            r"if\s*\(success\)\s*\{(?P<if_block>[\s\S]*?)\}\s*else\s*\{(?P<else_block>[\s\S]*?)\}\s*}\s*catch",
            shell_html,
        )
        self.assertIsNotNone(success_flow, "Could not locate pickDataDirectory success/cancel flow")

        if_block = success_flow.group("if_block")
        else_block = success_flow.group("else_block")

        self.assertIn("showCanvas();", if_block)
        self.assertNotIn("showCanvas();", else_block)


if __name__ == "__main__":
    unittest.main()
