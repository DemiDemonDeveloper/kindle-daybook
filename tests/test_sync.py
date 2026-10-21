"""Exercise the real curl child transport using a local fake curl executable."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class SyncTests(unittest.TestCase):
    def test_private_journal_command_and_reply(self):
        with tempfile.TemporaryDirectory() as folder:
            tmp = Path(folder)
            data = tmp / 'data'
            data.mkdir()
            (data / 'config.json').write_text(json.dumps({
                'telegram_token': '123:synthetic_token', 'telegram_chat_id': '42'}))
            curl = tmp / 'curl'
            curl.write_text('''#!/usr/bin/env python3
import sys,json
from pathlib import Path
conf=sys.stdin.read()
output=Path(sys.argv[sys.argv.index('--output')+1])
if 'getUpdates' in conf:
    result=[{'update_id':100,'message':{'chat':{'id':42,'type':'private'},'text':'/journal Transport test entry'}}]
else:
    result={}
    Path(str(output)+'.reply').write_text('sent')
output.write_text(json.dumps({'ok':True,'result':result}))
''')
            curl.chmod(0o755)
            env={**os.environ,'PATH':str(tmp)+':'+os.environ['PATH']}
            result=subprocess.run([str(ROOT/'build/daybook-host'),'--data',str(data),'--sync-once'],env=env,text=True,capture_output=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('journal entries: 1',result.stdout)
            self.assertTrue((data/'telegram-response.json.reply').exists())
            self.assertTrue((data/'state.bin').read_bytes().startswith(b'DBKSTR01'))
            replay=subprocess.run([str(ROOT/'build/daybook-host'),'--data',str(data),'--sync-once'],env=env,text=True,capture_output=True,timeout=10)
            self.assertEqual(replay.returncode,0,replay.stderr)
            self.assertIn('journal entries: 1',replay.stdout)


if __name__=='__main__':
    unittest.main()
