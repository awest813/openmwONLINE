import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class IntegrationTestsScriptTests(unittest.TestCase):
    def _prepare_example_suite(self, temp_dir):
        example_suite = temp_dir / "example_suite"
        for relative in [
            "game_template/data",
            "example_animated_creature/data",
            "the_hub/data",
            "integration_tests/data",
            "example_static_props/data",
        ]:
            (example_suite / relative).mkdir(parents=True, exist_ok=True)
        for relative_file in [
            "game_template/data/template.omwgame",
            "example_animated_creature/data/landracer.omwaddon",
            "the_hub/data/the_hub.omwaddon",
            "integration_tests/data/mwscript.omwaddon",
            "settings.cfg",
        ]:
            (example_suite / relative_file).write_text("")
        return example_suite

    def test_missing_test_config_is_reported_without_traceback(self):
        repo_root = Path(__file__).resolve().parents[2]
        integration_tests_data = repo_root / "scripts" / "data" / "integration_tests"
        test_dir = integration_tests_data / "test_missing_cfg_regression"

        temp_dir = Path(tempfile.mkdtemp())
        try:
            example_suite = self._prepare_example_suite(temp_dir)

            openmw_mock = temp_dir / "openmw_mock.py"
            openmw_mock.write_text('#!/usr/bin/env python3\nprint("Quit requested by a Lua script")\n')
            openmw_mock.chmod(0o755)

            test_dir.mkdir(parents=True, exist_ok=True)

            result = subprocess.run(
                [
                    "python3",
                    str(repo_root / "scripts" / "integration_tests.py"),
                    str(example_suite),
                    "--omw",
                    str(openmw_mock),
                    "--workdir",
                    str(temp_dir / "out"),
                ],
                cwd=repo_root,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("fatal error: missing test config", result.stdout)
            self.assertNotIn("Traceback", result.stderr)
        finally:
            if test_dir.exists():
                shutil.rmtree(test_dir)
            shutil.rmtree(temp_dir)

    def test_orphan_test_ok_line_is_reported_without_traceback(self):
        repo_root = Path(__file__).resolve().parents[2]
        integration_tests_data = repo_root / "scripts" / "data" / "integration_tests"
        test_dir = integration_tests_data / "test_orphan_ok_regression"

        temp_dir = Path(tempfile.mkdtemp())
        try:
            example_suite = self._prepare_example_suite(temp_dir)

            openmw_mock = temp_dir / "openmw_mock.py"
            openmw_mock.write_text(
                '#!/usr/bin/env python3\n'
                'print("TEST_OK 0\\torphan")\n'
                'print("Quit requested by a Lua script")\n'
            )
            openmw_mock.chmod(0o755)

            test_dir.mkdir(parents=True, exist_ok=True)
            (test_dir / "openmw.cfg").write_text("")

            result = subprocess.run(
                [
                    "python3",
                    str(repo_root / "scripts" / "integration_tests.py"),
                    str(example_suite),
                    "--omw",
                    str(openmw_mock),
                    "--workdir",
                    str(temp_dir / "out"),
                ],
                cwd=repo_root,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("orphan TEST_OK line", result.stdout)
            self.assertNotIn("Traceback", result.stdout)
            self.assertNotIn("Traceback", result.stderr)
        finally:
            if test_dir.exists():
                shutil.rmtree(test_dir)
            shutil.rmtree(temp_dir)


if __name__ == "__main__":
    unittest.main()
