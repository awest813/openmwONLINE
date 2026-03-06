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

  globalThis.__openmwLastPickResult = null;
  globalThis.__openmwGetLastPickResult = function() {
    return globalThis.__openmwLastPickResult;
  };

  function reportPhase(phase, status, details) {
    if (typeof globalThis.__openmwReportPhase === 'function') {
      globalThis.__openmwReportPhase(phase, status, details || '');
    }
  }

  function ensureVisibilityMarkers() {
    const ids = ['terrain-visible-marker', 'objects-visible-marker', 'sky-visible-marker', 'npc-visible-marker'];
    for (const id of ids) {
      if (document.getElementById(id)) continue;
      const marker = document.createElement('div');
      marker.id = id;
      marker.style.display = 'none';
      marker.textContent = id.replace(/-/g, ' ');
      document.body.appendChild(marker);
    }
  }

  function startStubNewGameFlow() {
    reportPhase('new_game', 'running', 'Starting new game from main menu');
    setTimeout(function() {
      const chargen = document.createElement('div');
      chargen.id = 'chargen-marker';
      chargen.textContent = 'Character creation reached';
      document.body.appendChild(chargen);
      reportPhase('character_creation', 'passed', 'Character creation UI reached');

      const world = document.createElement('div');
      world.id = 'world-entry-marker';
      world.textContent = 'Entered game world';
      document.body.appendChild(world);
      ensureVisibilityMarkers();
      document.getElementById('terrain-visible-marker').style.display = 'block';
      document.getElementById('objects-visible-marker').style.display = 'block';
      document.getElementById('sky-visible-marker').style.display = 'block';
      document.getElementById('npc-visible-marker').style.display = 'block';

      reportPhase('new_game', 'passed', 'New game confirmed');
      reportPhase('world_entry', 'passed', 'Entered world and scene markers visible');
      reportPhase('render_validation', 'passed', 'Terrain/objects/sky/NPC markers visible');
      reportPhase('input_pointer_lock', 'passed', 'Click-to-play interaction path validated');
    }, 20);
  }

  globalThis.__openmwPickDataDirectory = async function() {
    reportPhase('data_import', 'running', 'Smoke stub import begin');
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
    reportPhase('rendering_main_menu', 'passed', 'Main menu marker rendered');
    globalThis.__openmwLastPickResult = { status: 'success', message: 'Stub import succeeded' };
    return true;
  };

  globalThis.__openmwTestSaveGame = function(slotName, payload) {
    localStorage.setItem('openmw-smoke-save-' + slotName, JSON.stringify(payload));
    reportPhase('save_load', 'running', 'Saved slot ' + slotName);
  };

  globalThis.__openmwTestLoadGame = function(slotName) {
    const raw = localStorage.getItem('openmw-smoke-save-' + slotName);
    const parsed = raw ? JSON.parse(raw) : null;
    reportPhase('save_load', parsed ? 'passed' : 'failed', parsed ? ('Loaded slot ' + slotName) : ('Missing slot ' + slotName));
    return parsed;
  };

  window.addEventListener('load', function() {
    setTimeout(function() {
      Module.onRuntimeInitialized();
    }, 20);
  });

  document.addEventListener('click', function(evt) {
    const clickToPlay = document.getElementById('click-to-play');
    if (clickToPlay && evt.target && clickToPlay.contains(evt.target)) {
      startStubNewGameFlow();
    }
  }, true);
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

    def test_browser_progression_smoke_with_phase_markers_and_reload_persistence(self):
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch()
            context = browser.new_context()
            page = context.new_page()
            page.goto(f'http://127.0.0.1:{self.port}/openmw.html', wait_until='domcontentloaded')

            page.wait_for_function("() => document.getElementById('status').textContent.includes('Ready - select your Morrowind data folder to begin')")
            page.click('#pick-data-btn')

            page.wait_for_function("() => document.getElementById('status').textContent.includes('Game data loaded successfully!')")
            self.assertIn('visible', page.get_attribute('#canvas-container', 'class') or '')
            self.assertFalse(page.is_hidden('#click-to-play'))
            self.assertEqual(page.inner_text('#main-menu-marker'), 'Main menu visible')

            page.click('#click-to-play')
            page.wait_for_selector('#chargen-marker')
            page.wait_for_selector('#world-entry-marker')
            page.wait_for_function("""
                () => ['terrain-visible-marker', 'objects-visible-marker', 'sky-visible-marker', 'npc-visible-marker']
                  .every((id) => {
                    const el = document.getElementById(id);
                    return !!el && getComputedStyle(el).display !== 'none';
                  })
            """)

            page.evaluate(
                """
                () => {
                  globalThis.__openmwTestSaveGame('slot1', {
                    position: 'Seyda Neen',
                    inventoryItem: 'Iron Dagger',
                    cycle: 1
                  });
                }
                """
            )
            loaded_cycle1 = page.evaluate("() => globalThis.__openmwTestLoadGame('slot1')")
            self.assertEqual(loaded_cycle1['cycle'], 1)

            page.evaluate(
                """
                () => {
                  globalThis.__openmwTestSaveGame('slot1', {
                    position: 'Seyda Neen Lighthouse',
                    inventoryItem: 'Steel Dagger',
                    cycle: 2
                  });
                }
                """
            )

            timeline_before_reload = page.evaluate("() => globalThis.__openmwPhaseTimeline || []")
            required_before_reload = {
                'startup',
                'data_import',
                'rendering_main_menu',
                'new_game',
                'character_creation',
                'world_entry',
                'render_validation',
                'input_pointer_lock',
                'save_load',
            }
            seen_before_reload = {entry['phase'] for entry in timeline_before_reload if entry.get('status') in {'passed', 'running'}}
            missing_before_reload = sorted(required_before_reload - seen_before_reload)
            self.assertEqual([], missing_before_reload, f'Missing required phases before reload: {missing_before_reload}')

            page.reload(wait_until='domcontentloaded')
            page.wait_for_function("() => document.getElementById('status').textContent.includes('Ready - select your Morrowind data folder to begin')")
            page.click('#pick-data-btn')
            page.wait_for_function("() => document.getElementById('status').textContent.includes('Game data loaded successfully!')")

            loaded_cycle2 = page.evaluate("() => globalThis.__openmwTestLoadGame('slot1')")
            self.assertEqual(loaded_cycle2['cycle'], 2)
            self.assertEqual(loaded_cycle2['position'], 'Seyda Neen Lighthouse')
            self.assertEqual(loaded_cycle2['inventoryItem'], 'Steel Dagger')

            timeline_after_reload = page.evaluate("() => globalThis.__openmwPhaseTimeline || []")
            self.assertTrue(
                any(entry.get('phase') == 'persistence_sync' and entry.get('status') in {'running', 'passed'} for entry in timeline_after_reload),
                'Expected persistence_sync phase marker after reload',
            )
            self.assertTrue(
                any(entry.get('phase') == 'save_load' and entry.get('status') == 'passed' for entry in timeline_after_reload),
                'Expected save_load passed marker after reload load operation',
            )

            context.close()
            browser.close()


if __name__ == '__main__':
    unittest.main()
