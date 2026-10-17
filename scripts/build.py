#!/usr/bin/env python3
"""Reconstruct and build Alyx Turnip in a new, isolated directory (Linux/WSL)."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / 'dependencies/native-build/quest3-turnip/optimization-mk2-compiler-combined'


def sha(path):
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def run(command, cwd=None):
    print('+', ' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, check=True)


def verified(path, expected):
    if sha(path) != expected:
        raise RuntimeError('SHA256 mismatch: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    variants = json.loads((ROOT / 'provenance/variants.json').read_text())
    parser.add_argument('--variant', choices=variants, default='compat',
                        help='Driver variant (default: historical compat reference)')
    parser.add_argument('--out', required=True, type=Path, help='New directory; existing paths are rejected')
    parser.add_argument('--archive', type=Path, help='Optional cached, pinned Mesa tar.gz')
    parser.add_argument('--ndk', type=Path, help='Extracted Android NDK r27d directory')
    parser.add_argument('--prepare-only', action='store_true', help='Verify and reconstruct sources without compiling')
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    if os.name != 'posix':
        parser.error('Use Linux or WSL with Python 3.12+')
    out = args.out.resolve()
    if out.exists() or out == ROOT or ROOT.is_relative_to(out):
        parser.error('Choose a new isolated output directory')
    if not 1 <= args.jobs <= 64:
        parser.error('--jobs must be between 1 and 64')
    if not args.prepare_only and not args.ndk:
        parser.error('--ndk is required for compilation')
    variant = variants[args.variant]
    manifest = json.loads((ROOT / variant['manifest']).read_text())
    release = json.loads((ROOT / 'provenance/release.json').read_text())
    for item in manifest['patches'] + manifest['inputs']:
        verified(ROOT / item['path'], item['sha256'])
    verified(ROOT / 'patches/0000-quest-baseline.patch', release['baselinePatchSha256'])
    if variant['sha256'] != manifest['candidateSha256'] or variant['patchCount'] != len(manifest['patches']):
        raise RuntimeError('Variant manifest identity mismatch')
    verified(ROOT / variant['binary'], variant['sha256'])
    out.mkdir(parents=True)
    archive_path = args.archive.resolve() if args.archive else out / 'mesa.tar.gz'
    if not args.archive:
        upstream = json.loads((ROOT / 'provenance/upstream-inputs.json').read_text())
        url = next(item['url'] for item in upstream if item['file'] == 'mesa.tar.gz')
        print('Downloading pinned Mesa source:', url, flush=True)
        urllib.request.urlretrieve(url, archive_path)
    verified(archive_path, release['mesaArchiveSha256'])
    source = out / 'mesa'
    source.mkdir()
    with tarfile.open(archive_path) as archive:
        for entry in archive:
            parts = entry.name.split('/', 1)
            if len(parts) != 2 or not parts[1]:
                continue
            entry.name = parts[1]
            archive.extract(entry, source, filter='data')
    # git apply must operate outside any surrounding repository.
    run(['git', 'init', '--quiet', source])
    run(['git', 'apply', ROOT / 'patches/0000-quest-baseline.patch'], source)
    for rel, expected in manifest['originalFiles'].items():
        path = source / rel
        if expected is None:
            if path.exists():
                raise RuntimeError('Unexpected baseline file: ' + rel)
        else:
            verified(path, expected)
    for item in manifest['patches']:
        # The historical profile patch embeds its original workstation path.
        # Generate the same flags with the local, checksummed profile instead.
        if item['path'].endswith('/compiler-profile.patch'):
            continue
        run(['git', 'apply', ROOT / item['path']], source)
        # Check the shared VRS parent before the optional MSAA/cache patches.
        if item['path'] == 'tests/turnip_vrs/experimental-fdm-vrs.patch':
            for name in ('tu_alyx_fdm_vrs.h', 'tu_alyx_fdm_vrs.inc'):
                if (source / 'src/freedreno/vulkan' / name).read_bytes() != (ROOT / 'tests/turnip_vrs' / name).read_bytes():
                    raise RuntimeError('VRS input mismatch: ' + name)
    helper_spec = importlib.util.spec_from_file_location('profile_patch', PACKAGE / 'compiler_profile_patch.py')
    helper = importlib.util.module_from_spec(helper_spec)
    helper_spec.loader.exec_module(helper)
    profile_patch = out / 'compiler-profile.patch'
    profile_patch.write_text(helper.make_patch(source, PACKAGE / 'compiler.profdata', True, 'cortex-a78c'))
    run(['git', 'apply', profile_patch], source)
    report = {'variant': args.variant, 'mesaRevision': release['mesaRevision'], 'sourcesPrepared': True,
              'referenceDriverSha256': variant['sha256'], 'patchCount': len(manifest['patches']),
              'compilerProfileSha256': release['profileSha256'],
              'gpuValidated': False, 'compiled': False}
    report_path = out / 'build-report.json'
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    if args.prepare_only:
        print('Verified source reconstruction:', source)
        return
    ndk = args.ndk.resolve()
    if 'Pkg.Revision = 27.3.13750724' not in (ndk / 'source.properties').read_text():
        raise RuntimeError('Android NDK r27d (27.3.13750724) is required')
    toolchain = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    if any(char in str(toolchain) for char in "'\n\r"):
        raise RuntimeError('Unsupported quote/newline in NDK path')
    cross = out / 'android-cross.ini'
    cross.write_text(f"""[binaries]
ar = '{toolchain}/llvm-ar'
c = '{toolchain}/aarch64-linux-android29-clang'
cpp = '{toolchain}/aarch64-linux-android29-clang++'
strip = '{toolchain}/llvm-strip'
pkg-config = '/usr/bin/false'
[built-in options]
c_args = ['-O2']
cpp_args = ['-O2']
cpp_link_args = ['-static-libstdc++']
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8'
endian = 'little'
""")
    run(['meson', 'setup', 'build-android', '--cross-file', cross,
         '-Dplatforms=android', '-Dplatform-sdk-version=34', '-Dandroid-stub=true',
         '-Dgallium-drivers=', '-Dvulkan-drivers=freedreno', '-Dfreedreno-kmds=kgsl',
         '-Degl=disabled', '-Dgles1=disabled', '-Dgles2=disabled', '-Dopengl=false',
         '-Dglx=disabled', '-Dglvnd=disabled', '-Dllvm=disabled', '-Dshared-glapi=disabled',
         '-Dvulkan-layers=', '-Dbuildtype=release', '-Dstrip=true', '-Db_lto=false', '-Dcpp_rtti=false'], source)
    run(['ninja', '-C', source / 'build-android', '-j' + str(args.jobs),
         'src/freedreno/vulkan/libvulkan_freedreno.so'])
    binary = out / 'libalyx_turnip.so'
    shutil.copyfile(source / 'build-android/src/freedreno/vulkan/libvulkan_freedreno.so', binary)
    report.update(compiled=True, driverSha256=sha(binary), byteIdenticalToReference=sha(binary) == variant['sha256'])
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
