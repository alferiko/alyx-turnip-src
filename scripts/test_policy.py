#!/usr/bin/env python3
"""Test the VRS header from a reconstructed variant under ASan/UBSan (Linux)."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesa', required=True, type=Path)
args = parser.parse_args()
mesa = args.mesa.resolve()
report = json.loads((mesa.parent / 'build-report.json').read_text())
headers = mesa / 'src/freedreno/vulkan'
tests = ['policy_test.cc']
if report['variant'] == 'msaa-vrs-cache':
    tests.append('final_map_cache_test.cc')
with tempfile.TemporaryDirectory(prefix='alyx-policy-') as directory:
    temporary = Path(directory)
    for test in tests:
        # Quoted includes search the test's own directory first. Copy the
        # unchanged historical test away from the repository's baseline header.
        source = temporary / test
        shutil.copyfile(root / 'tests/turnip_vrs' / test, source)
        binary = temporary / source.stem
        subprocess.run(['g++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-I' + str(headers), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('PASS reconstructed variant policy:', report['variant'])
