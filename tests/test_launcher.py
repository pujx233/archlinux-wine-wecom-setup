"""Exercise launcher path handling without starting Wine or any desktop helper."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class LauncherPathsTest(unittest.TestCase):
    def test_relative_prefix_is_resolved_before_changing_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            prefix = root / 'wine prefix'
            scripts = prefix / 'wecom-compat/scripts'
            scripts.mkdir(parents=True)
            configs = scripts.parent / 'configs'
            configs.mkdir()
            (configs / 'dll-overrides.txt').write_text('winemenubuilder.exe=d\n')
            (prefix / 'system.reg').touch()
            app = prefix / 'drive_c/Program Files (x86)/WXWork/WXWork.exe'
            app.parent.mkdir(parents=True)
            app.touch()
            launcher = scripts / 'launch-wecom.sh'
            shutil.copyfile(ROOT / 'scripts/launch-wecom.sh', launcher)
            # The marker proves startup reached the helper with the correct path.
            # The deliberately absent titlebar executable then stops the launcher
            # at its existing mandatory-file check, before it can invoke Wine.
            (scripts / 'prepare-startup.py').write_text(
                'import pathlib, sys\n'
                'prefix = pathlib.Path(sys.argv[2])\n'
                '(prefix / "observed-prefix").write_text(str(prefix))\n')
            alias = root / 'prefix-link'
            alias.symlink_to(prefix, target_is_directory=True)
            environment = dict(os.environ, WECOM_PREFIX='./prefix-link',
                               WECOM_TRAY_ICON_FIX='0', WECOM_TITLEBAR_FIX='1')
            result = subprocess.run(['bash', str(launcher)], cwd=root,
                                    env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertEqual((prefix / 'observed-prefix').read_text(), str(prefix))
            self.assertTrue((prefix / 'wecom-startup.log').is_file())
            self.assertEqual(list(app.parent.iterdir()), [app])


if __name__ == '__main__':
    unittest.main()
