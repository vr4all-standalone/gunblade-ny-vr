"""Run the real allowlisted settings copier against isolated filesystem fixtures."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / 'android/app/src/main/java/com/vrlightgunhub/gunbladeny'
JDK = Path(os.environ.get('JAVA_HOME', 'E:/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2'))


class SettingsBackupTests(unittest.TestCase):
    def test_originals_allowlist_absence_bounds_collisions_and_containment(self):
        source = SOURCES / 'SettingsBackup.java'
        self.assertTrue(source.is_file(), 'Read-only settings backup is missing')
        temporary_root = ROOT / 'build/test-temp/android-settings-backup'
        temporary_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temporary_root) as temporary:
            work = Path(temporary)
            classes = work / 'classes'
            classes.mkdir()
            compile_result = subprocess.run([
                str(JDK / 'bin/javac.exe'), '-encoding', 'UTF-8', '-d', str(classes),
                str(source), str(ROOT / 'android/tests/SettingsBackupHarness.java')],
                capture_output=True, text=True)
            self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
            result = subprocess.run([
                str(JDK / 'bin/java.exe'), '-cp', str(classes),
                'com.vrlightgunhub.gunbladeny.SettingsBackupHarness', str(work / 'fixtures')],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            if result.stdout.strip():
                print(result.stdout.strip())
            # Independently parse manifests and verify both hashes and every exported payload.
            for snapshot in (work / 'fixtures/success/external').glob('settings-backup-*'):
                manifest = json.loads((snapshot / 'manifest.json').read_text(encoding='utf-8'))
                self.assertTrue(manifest['complete'])
                self.assertEqual(manifest['roots'], {
                    'settings.ini': 'present', 'cfg': 'present', 'nvram': 'present'})
                self.assertEqual({item['path'] for item in manifest['files']}, {
                    'settings.ini', 'cfg/gunblade.cfg', 'nvram/nested/scores.bin'})
                for item in manifest['files']:
                    original = (work / 'fixtures/success/private' / item['path']).read_bytes()
                    copied = (snapshot / item['path']).read_bytes()
                    self.assertEqual(original, copied)
                    self.assertEqual(item['originalLength'], len(original))
                    self.assertEqual(item['copiedLength'], len(copied))
                    self.assertEqual(item['originalSha256'], hashlib.sha256(original).hexdigest())
                    self.assertEqual(item['copiedSha256'], hashlib.sha256(copied).hexdigest())


if __name__ == '__main__':
    unittest.main()
