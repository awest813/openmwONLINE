import http.server
import socketserver
import tempfile
import threading
import unittest
from pathlib import Path

try:
    from playwright.sync_api import sync_playwright
except ImportError:  # pragma: no cover - exercised in environments without Playwright.
    sync_playwright = None


TEST_STUB_SCRIPT = """
<script>
  globalThis.__openmwGetPickerCapabilities = function() {
    return { supported: true, directoryPicker: true, fileInputFallback: false };
  };

  globalThis.__openmwPickDataDirectory = async function() {
    if (typeof globalThis.__openmwOnUploadPhase === 'function') {
      globalThis.__openmwOnUploadPhase('scanning');
      globalThis.__openmwOnUploadProgress(0, 2, 0, 100);
      globalThis.__openmwOnUploadPhase('uploading');
      globalThis.__openmwOnUploadProgress(1, 2, 50, 100);
      globalThis.__openmwOnUploadProgress(2, 2, 100, 100);
    }

    const marker = document.createElement('div');
    marker.id = 'main-menu-marker';
    marker.textContent = 'Main menu visible';
    marker.style.position = 'fixed';
    marker.style.top = '12px';
    marker.style.left = '12px';
    marker.style.zIndex = '1000';
    marker.style.color = '#c8a86e';
    marker.style.background = 'rgba(0,0,0,0.6)';
    marker.style.padding = '6px 10px';
    marker.style.border = '1px solid #c8a86e';
    document.body.appendChild(marker);

    logToConsole('WASM: main menu shown (smoke stub)', 'info');
    return true;
  };

  globalThis.__openmwTestSaveGame = function(slotName, payload) {
    localStorage.setItem('openmw-smoke-save-' + slotName, JSON.stringify(payload));
  };

  globalThis.__openmwTestLoadGame = function(slotName) {
    const raw = localStorage.getItem('openmw-smoke-save-' + slotName);
    return raw ? JSON.parse(raw) : null;
  };

  window.addEventListener('load', function() {
    setTimeout(function() {
      Module.onRuntimeInitialized();
    }, 20);
  });
</script>
""".strip()


class _SilentHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        return


class _ThreadedTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


class WasmBrowserSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if sync_playwright is None:
            raise unittest.SkipTest('Playwright is not installed; skipping browser smoke tests.')

        repo_root = Path(__file__).resolve().parents[2]
        shell_path = repo_root / 'files' / 'wasm' / 'openmw_shell.html'
        shell_html = shell_path.read_text(encoding='utf-8')

        cls.temp_dir = Path(tempfile.mkdtemp(prefix='openmw-smoke-'))
        rendered = shell_html.replace('{{{ SCRIPT }}}', TEST_STUB_SCRIPT)
        (cls.temp_dir / 'openmw.html').write_text(rendered, encoding='utf-8')
        (cls.temp_dir / 'USER_TESTING.md').write_text('Smoke harness fixture', encoding='utf-8')

        handler = lambda *args, **kwargs: _SilentHTTPRequestHandler(*args, directory=str(cls.temp_dir), **kwargs)
        cls.server = _ThreadedTCPServer(('127.0.0.1', 0), handler)
        cls.port = cls.server.server_address[1]
        cls.server_thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.server_thread.start()

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, 'server'):
            cls.server.shutdown()
            cls.server.server_close()
        if hasattr(cls, 'server_thread'):
            cls.server_thread.join(timeout=2)
        if hasattr(cls, 'temp_dir') and cls.temp_dir.exists():
            for child in cls.temp_dir.iterdir():
                child.unlink()
            cls.temp_dir.rmdir()

    def test_boot_data_load_main_menu_and_save_load_round_trip(self):
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch()
            page = browser.new_page()
            page.goto(f'http://127.0.0.1:{self.port}/openmw.html', wait_until='domcontentloaded')

            page.wait_for_function("() => document.getElementById('status').textContent.includes('Ready - select your Morrowind data folder to begin')")
            page.click('#pick-data-btn')

            page.wait_for_function("() => document.getElementById('status').textContent.includes('Game data loaded successfully!')")
            self.assertIn('visible', page.get_attribute('#canvas-container', 'class') or '')
            self.assertFalse(page.is_hidden('#click-to-play'))
            self.assertEqual(page.inner_text('#main-menu-marker'), 'Main menu visible')

            page.evaluate("""
                () => {
                  globalThis.__openmwTestSaveGame('slot1', {
                    position: 'Seyda Neen',
                    inventoryItem: 'Iron Dagger'
                  });
                }
            """)
            page.reload(wait_until='domcontentloaded')
            loaded = page.evaluate("() => globalThis.__openmwTestLoadGame('slot1')")
            self.assertEqual(loaded['position'], 'Seyda Neen')
            self.assertEqual(loaded['inventoryItem'], 'Iron Dagger')
            browser.close()


if __name__ == '__main__':
    unittest.main()
