#!/usr/bin/env python3
"""Pair a fresh Telegram bot with the owner; secrets stay out of arguments/logs."""
import argparse
import getpass
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import time


def api(token, method, payload):
    config = 'url = ' + json.dumps(f'https://api.telegram.org/bot{token}/{method}') + '\n'
    config += 'header = "Content-Type: application/json"\n'
    config += 'data = ' + json.dumps(json.dumps(payload)) + '\n'
    result = subprocess.run(
        ['curl', '--silent', '--show-error', '--fail', '--connect-timeout', '8',
         '--max-time', '25', '--config', '-'],
        input=config, text=True, capture_output=True,
    )
    if result.returncode:
        raise SystemExit('Telegram request failed. Check the token and internet connection; no token was logged.')
    try:
        data = json.loads(result.stdout)
    except ValueError:
        raise SystemExit('Telegram returned an unreadable response.') from None
    if not data.get('ok'):
        raise SystemExit('Telegram rejected the request. Use a fresh bot token.')
    return data['result']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True, help='Kindle daybook/data folder')
    parser.add_argument('--mac-dialog', action='store_true', help='Use a hidden native Mac prompt for the token')
    args = parser.parse_args()
    target = args.data / 'config.json'
    if target.exists():
        raise SystemExit('A config already exists. Back it up and remove it before deliberately pairing a new bot.')
    print('Create a NEW bot in Telegram using @BotFather and /newbot.')
    if args.mac_dialog:
        prompt = 'text returned of (display dialog "Paste the Telegram bot token from BotFather. It will be stored only in your Kindle Daybook settings." with title "Connect Daybook to Telegram" default answer "" with hidden answer buttons {"Cancel", "Connect"} default button "Connect")'
        result = subprocess.run(['osascript', '-e', prompt], text=True, capture_output=True)
        if result.returncode:
            raise SystemExit('Pairing cancelled. No token was saved.')
        token = result.stdout.strip()
    else:
        token = getpass.getpass('Paste its bot token (hidden): ').strip()
    if not re.fullmatch(r'[0-9]+:[A-Za-z0-9_-]+', token):
        raise SystemExit('The token format looks incorrect.')
    bot = api(token, 'getMe', {})
    if api(token, 'getWebhookInfo', {}).get('url'):
        raise SystemExit('This bot already has a webhook. Use a fresh bot; its existing connection was not changed.')
    pairing = 'daybook_' + secrets.token_hex(12)
    print(f"Open @{bot['username']} in your own private Telegram chat and send:")
    print('/start ' + pairing)
    deadline, offset, owner = time.monotonic() + 180, 0, None
    while time.monotonic() < deadline and owner is None:
        updates = api(token, 'getUpdates', {'offset': offset, 'timeout': 10, 'limit': 100, 'allowed_updates': ['message']})
        for update in updates:
            offset = max(offset, update['update_id'] + 1)
            msg = update.get('message', {})
            if msg.get('chat', {}).get('type') == 'private' and msg.get('text') == '/start ' + pairing:
                owner = str(msg['chat']['id'])
                break
    if owner is None:
        raise SystemExit('Pairing timed out. No config was written; run setup again.')
    args.data.mkdir(parents=True, exist_ok=True)
    config = {'telegram_token': token, 'telegram_chat_id': owner, 'poll_seconds': 30,
              'timezone': 'CST6CDT,M3.2.0,M11.1.0'}
    temporary = target.with_suffix('.tmp')
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as stream:
        json.dump(config, stream, indent=2)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(target)
    print('Paired successfully. Safely eject the Kindle, disconnect USB, and launch Daybook.')
    print('Keep config.json private. USB storage can expose it to anyone with physical access.')


if __name__ == '__main__':
    main()
