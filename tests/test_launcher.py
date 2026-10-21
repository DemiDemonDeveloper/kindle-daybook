"""Check cleanup paths with a dummy process, without touching a Kindle."""
from pathlib import Path
import os
import signal
import sys
import time
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class LauncherTests(unittest.TestCase):
    def test_normal_error_and_probe_failure_restore(self):
        source = (ROOT / 'scripts/run-dashboard.sh').read_text()
        self.assertNotIn('stop framework', source)
        self.assertNotIn('start framework', source)
        for mode, expected in [('normal', 0), ('error', 7), ('probe-failure', 1)]:
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                tmp = Path(directory)
                app = tmp / 'daybook'
                (app / 'data').mkdir(parents=True)
                (app / 'bin').mkdir()
                tools = tmp / 'tools'
                tools.mkdir()
                framebuffer = tmp / 'framebuffer'
                framebuffer.write_bytes(b'original framebuffer')
                heartbeat=tmp/'heartbeat'
                heartbeat.write_text('0')
                program='from pathlib import Path;import sys,time; p=Path(sys.argv[1]);i=0\nwhile True:\n i+=1;t=p.with_suffix(".tmp");t.write_text(str(i));t.replace(p);time.sleep(.02)'
                worker = subprocess.Popen([sys.executable,'-c',program,str(heartbeat)])
                try:
                    files = {
                        app / 'bin/daybook': f'#!/bin/sh\nif [ "$3" = "--probe" ]; then exit {3 if mode == "probe-failure" else 0}; fi\nexit {7 if mode == "error" else 0}\n',
                        tools / 'pidof': f'#!/bin/sh\necho {worker.pid}\n',
                        tools / 'lipc-set-prop': '#!/bin/sh\nexit 0\n',
                        tools / 'eips': '#!/bin/sh\nexit 0\n',
                        tmp / 'libkh/bin/fbink': '#!/bin/sh\nexit 0\n',
                    }
                    for p, text in files.items():
                        p.parent.mkdir(parents=True, exist_ok=True)
                        p.write_text(text)
                        p.chmod(0o755)
                    launcher = tmp / 'launcher.sh'
                    launcher.write_text(source.replace('APP=/mnt/us/daybook', 'APP=' + str(app)).replace('/dev/fb0', str(framebuffer)))
                    result = subprocess.run(['sh', str(launcher)], env={**os.environ, 'PATH': str(tools) + ':' + os.environ['PATH']}, timeout=10)
                    self.assertEqual(result.returncode, expected)
                    before=int(heartbeat.read_text());time.sleep(.15);after=int(heartbeat.read_text());self.assertGreater(after,before)
                    self.assertEqual(framebuffer.read_bytes(), b'original framebuffer')
                    self.assertFalse((app / 'data/launcher-lock').exists())
                finally:
                    os.kill(worker.pid, signal.SIGCONT)
                    worker.terminate()
                    worker.wait(timeout=3)


if __name__ == '__main__':
    unittest.main()
