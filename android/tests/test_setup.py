"""Execute production setup policy and ROM diagnostics without a headset."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT/'android/app/src/main/java/com/vrlightgunhub/gunbladeny'
JDK = Path(os.environ.get('JAVA_HOME', 'E:/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2'))
ANDROID = '{http://schemas.android.com/apk/res/android}'
TEMP = ROOT/'build/test-temp/android-setup'


class SetupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        TEMP.mkdir(parents=True, exist_ok=True)

    def test_setup_requests_the_shared_wide_resizable_panel(self):
        app = ET.parse(ROOT/'android/app/src/main/AndroidManifest.xml').find('application')
        setup = next(item for item in app.findall('activity') if item.get(ANDROID+'name') == '.SetupActivity')
        self.assertEqual(setup.get(ANDROID+'resizeableActivity'), 'true')
        layout = setup.find('layout')
        self.assertIsNotNone(layout, 'Quest preparation panel dimensions are missing')
        self.assertEqual(layout.get(ANDROID+'defaultWidth'), '860dp')
        self.assertEqual(layout.get(ANDROID+'defaultHeight'), '640dp')

    def test_validated_entry_return_pause_and_retry_policy(self):
        source = SOURCES/'ReadyLaunchPolicy.java'
        self.assertTrue(source.is_file(), 'Validated automatic-entry policy is missing')
        self.run_java([source, ROOT/'android/tests/LauncherPolicyHarness.java'],
                      'LauncherPolicyHarness')

    def test_missing_payload_reports_the_selected_app_folder(self):
        with tempfile.TemporaryDirectory(prefix='gunblade-rom-help-', dir=TEMP) as temporary:
            archive = Path(temporary)/'owned/roms/gunblade.zip'
            archive.parent.mkdir(parents=True)
            with zipfile.ZipFile(archive, 'w') as output:
                output.writestr('unrelated.bin', b'synthetic fixture')
            self.run_java([SOURCES/'GameFiles.java', ROOT/'android/tests/SetupFilesHarness.java'],
                          'SetupFilesHarness', [str(archive)])

    def test_asset_copy_failures_and_overlapping_workers_preserve_complete_files(self):
        source = SOURCES/'AssetInstaller.java'
        self.assertTrue(source.is_file(), 'Atomic asset installation is missing')
        with tempfile.TemporaryDirectory(prefix='gunblade-asset-install-', dir=TEMP) as temporary:
            self.run_java([source, ROOT/'android/tests/AssetInstallerHarness.java'],
                          'AssetInstallerHarness', [temporary])

    def run_java(self, sources, main, arguments=()):
        with tempfile.TemporaryDirectory(prefix='gunblade-setup-java-', dir=TEMP) as temporary:
            result = subprocess.run([str(JDK/'bin/javac.exe'), '-encoding', 'UTF-8', '-d', temporary,
                                     *map(str, sources)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            result = subprocess.run([str(JDK/'bin/java.exe'), '-cp', temporary,
                                     'com.vrlightgunhub.gunbladeny.'+main, *arguments],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)


if __name__ == '__main__':
    unittest.main()
