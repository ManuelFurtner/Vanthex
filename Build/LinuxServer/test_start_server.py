"""Exercise the real launcher with a fake server; no Unreal installation needed."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        shutil.copyfile(Path(__file__).with_name('StartServer.sh'), self.root / 'StartServer.sh')
        self.binary = self.root / 'Vanthex/Binaries/Linux/VanthexServer'
        self.binary.parent.mkdir(parents=True)
        self.binary.write_text('#!/bin/bash\nprintf "%s\\n" "$@"\nexit 23\n', encoding='utf-8')
        self.binary.chmod(0o755)
        self.bash = os.environ.get('VANTHEX_TEST_BASH', 'bash')

    def tearDown(self):
        self.temp.cleanup()

    def run_launcher(self, env=None):
        clean = dict(os.environ)
        clean.pop('VANTHEX_PORT', None)
        clean.pop('ARBITRIUM_PORT_GAMEPORT_INTERNAL', None)
        clean.update(env or {})
        return subprocess.run([self.bash, str(self.root / 'StartServer.sh'), '-Test=two words'],
                              env=clean, text=True, capture_output=True, timeout=15)

    def test_defaults_and_exit_status(self):
        result = self.run_launcher()
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertEqual(result.stdout.splitlines(), [
            '/Game/Vanthex/Maps/GridV4', '-port=7777', '-unattended', '-nosound',
            '-stdout', '-FullStdOutLogOutput', '-Test=two words'])

    def test_edgegap_port_precedes_local_setting(self):
        result = self.run_launcher({'VANTHEX_PORT': '18888', 'ARBITRIUM_PORT_GAMEPORT_INTERNAL': '19999'})
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertIn('-port=19999', result.stdout.splitlines())

    def test_local_port(self):
        result = self.run_launcher({'VANTHEX_PORT': '18888'})
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertIn('-port=18888', result.stdout.splitlines())

    def test_invalid_ports_do_not_launch(self):
        for value in ['0', '65536', '-1', '7777;echo bad', '999999999999999999999']:
            with self.subTest(port=value):
                result = self.run_launcher({'VANTHEX_PORT': value})
                self.assertEqual(result.returncode, 64)
                self.assertEqual(result.stdout, '')

    def test_missing_binary(self):
        self.binary.unlink()
        self.assertEqual(self.run_launcher().returncode, 66)


if __name__ == '__main__':
    unittest.main(verbosity=2)
