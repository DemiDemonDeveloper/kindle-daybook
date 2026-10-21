#!/usr/bin/env python3
"""Install a built Daybook alpha without overwriting task data or bot settings."""
import argparse
import hashlib
from pathlib import Path
import shutil
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--volume', type=Path, required=True)
    args = parser.parse_args()
    root = args.volume.resolve()
    project = Path(__file__).resolve().parent.parent
    if not (root / 'documents' / 'JAILBROKEN.txt').is_file():
        raise SystemExit('No jailbreak marker at this location. Check the selected Kindle volume.')
    files = ['daybook/bin/daybook', 'daybook/run-dashboard.sh', 'documents/Daybook.sh']
    for name in files:
        if not (project / 'dist' / name).is_file():
            raise SystemExit('Build missing; run make kindle first.')
    backup = project / 'build' / 'backups' / time.strftime('%Y%m%d-%H%M%S')
    for name in ['daybook', 'documents/Daybook.sh']:
        existing = root / name
        if existing.exists():
            dest = backup / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            if existing.is_dir():
                shutil.copytree(existing, dest)
            else:
                shutil.copy2(existing, dest)
    for name in files:
        source, target = project / 'dist' / name, root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o755)
        if digest(source) != digest(target):
            raise SystemExit('Verification failed; do not launch Daybook.')
    (root / 'daybook' / 'data').mkdir(exist_ok=True)
    print('Installed and verified 3 files. Existing tasks and settings preserved.')
    print('Automatic startup is disabled. Safely eject, unplug USB, and open Daybook in the Library.')


if __name__ == '__main__':
    main()
