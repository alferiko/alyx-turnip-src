#!/usr/bin/env python3
"""Refresh publication checksums after reviewing intended repository changes."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
names = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=root).decode().split('\0')
result = {}
for name in sorted(set(names)):
    if not name or name == 'provenance/checksums.json':
        continue
    path = root / name
    if path.is_file():
        result[name] = hashlib.sha256(path.read_bytes()).hexdigest()
(root / 'provenance/checksums.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(f'Checksums updated for {len(result)} files. Run scripts/verify.py next.')
