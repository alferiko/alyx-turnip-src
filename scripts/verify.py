#!/usr/bin/env python3
"""Verify all published files and the exact historical driver inputs offline."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
checksums = json.loads((root / 'provenance/checksums.json').read_text())
for name, expected in checksums.items():
    path = root / name
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit('Checksum mismatch: ' + name)
variants = json.loads((root / 'provenance/variants.json').read_text())
for name, variant in variants.items():
    manifest = json.loads((root / variant['manifest']).read_text())
    if variant['sha256'] != manifest['candidateSha256'] or variant['patchCount'] != len(manifest['patches']):
        raise SystemExit('Variant identity mismatch: ' + name)
    for item in manifest['patches'] + manifest['inputs'] + [dict(path=variant['binary'], sha256=variant['sha256'])]:
        if item['path'] not in checksums:
            raise SystemExit('Input missing from publication checksums: ' + item['path'])
        actual = hashlib.sha256((root / item['path']).read_bytes()).hexdigest()
        if actual != item['sha256']:
            raise SystemExit('Historical input mismatch: ' + item['path'])
    print(f'PASS {name}: {len(manifest["patches"])} patches, reference binary and all historical inputs')
print(f'PASS: {len(checksums)} published files; {len(variants)} driver variants.')
