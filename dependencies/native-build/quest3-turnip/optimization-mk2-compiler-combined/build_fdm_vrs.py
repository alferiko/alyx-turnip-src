"""Build the OFF-by-default A7xx FDM/VRS experiment over the exact release driver.

Run in the configured Linux/WSL Mesa environment, with its Meson venv on PATH.
The release patch manifest, baseline SO, and intermediate release SO are pinned.
Touched source files are backed up before mutation and restored in finally.
No APK or device is modified.
"""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tests/turnip_compile'))
from compiler_profile_patch import make_patch

BASELINE = '7171e6f718f582f08bb20e57a10c9e066d434241e984085cb3695187f51f90da'
RELEASE = 'abf6363b138f501b1a813ba5c030735282c307e74a59f14658e021ca9482f117'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mesa', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True,
                        help='New output directory, retained even after failure')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--cold-compile', action='store_true',
                        help='Add the isolated, runtime opt-in cold compiler experiment')
    parser.add_argument('--compile-alloc', action='store_true',
                        help='Add the isolated SSA allocation/empty-pass experiment')
    parser.add_argument('--compiler-o3', action='store_true',
                        help='Build NIR, IR3, SPIR-V and util CPU code at O3')
    parser.add_argument('--compiler-range-fastpath', action='store_true',
                        help='Avoid analyzer setup for known constant/cached range results')
    parser.add_argument('--ssa-pool-all', action='store_true',
                        help='Pool scratch SSA-use sets in the common IR3 builder')
    parser.add_argument('--deferred-constlen', action='store_true',
                        help='Compile safe-constant variants only when a complete pipeline selects them')
    profile = parser.add_mutually_exclusive_group()
    profile.add_argument('--compiler-profile-generate', action='store_true')
    profile.add_argument('--compiler-profile-use', type=Path)
    parser.add_argument('--compiler-thinlto', action='store_true')
    parser.add_argument('--compiler-cpu', choices=['cortex-a78c'])
    args = parser.parse_args()
    mesa, out = args.mesa.resolve(), args.out.resolve()
    repo = Path(__file__).resolve().parents[1]
    if out.exists() or out.is_relative_to(mesa):
        parser.error('Use a new output directory outside the configured Mesa tree')
    if not 1 <= args.jobs <= 64:
        parser.error('jobs must be 1..64')
    if args.ssa_pool_all and args.compile_alloc:
        parser.error('Generic SSA pool and the old shared-fold pool are separate experiments')
    if args.compiler_o3 and (args.compiler_profile_generate or args.compiler_profile_use):
        parser.error('Profile builds already include O3')
    if args.compiler_profile_use and not args.compiler_profile_use.is_file():
        parser.error('Compiler profile does not exist')
    if args.compiler_thinlto and not (args.compiler_profile_generate or args.compiler_profile_use):
        parser.error('ThinLTO experiment requires a profile build')
    if args.compiler_cpu and not (args.compiler_profile_generate or args.compiler_profile_use):
        parser.error('CPU target experiment requires a profile build')
    manifest_path = repo / 'dependencies/native-build/quest3-turnip/xemu-release-0.174/build.json'
    release = json.loads(manifest_path.read_text())
    if release['baselineSha256'] != BASELINE or release['candidateSha256'] != RELEASE:
        raise RuntimeError('Pinned release manifest mismatch')
    patches = release['patches'] + [dict(path='tests/turnip_vrs/experimental-fdm-vrs.patch')]
    if args.cold_compile:
        patches.append(dict(path='tests/turnip_compile/experimental-cold-compile.patch'))
    if args.compile_alloc:
        patches.append(dict(path='tests/turnip_compile/experimental-compile-alloc.patch'))
    if args.compiler_o3:
        patches.append(dict(path='tests/turnip_compile/experimental-compiler-o3.patch'))
    if args.compiler_range_fastpath:
        patches.append(dict(path='tests/turnip_compile/experimental-range-fastpath.patch'))
    if args.ssa_pool_all:
        patches.append(dict(path='tests/turnip_compile/experimental-ssa-pool-all.patch'))
    if args.deferred_constlen:
        patches.append(dict(path='tests/turnip_compile/experimental-deferred-constlen.patch'))
    out.mkdir(parents=True)
    if args.compiler_profile_generate or args.compiler_profile_use:
        profile_path = args.compiler_profile_use.resolve() if args.compiler_profile_use else None
        profile_patch = out / 'compiler-profile.patch'
        profile_patch.write_text(make_patch(mesa, profile_path, args.compiler_thinlto, args.compiler_cpu))
        patches.append(dict(path=str(profile_patch)))
    for item in patches:
        actual = sha(repo / item['path'])
        if item.get('sha256', actual) != actual:
            raise RuntimeError('Patch checksum mismatch: ' + item['path'])
        item['sha256'] = actual
    paths = set()
    for item in patches:
        for name in re.findall(r'^\+\+\+ b/(.+)$', (repo / item['path']).read_text(), re.M):
            path = (mesa / name).resolve()
            if not path.is_relative_to(mesa):
                raise RuntimeError('Patch escapes Mesa tree')
            paths.add(path)
    library = mesa / 'build-android/src/freedreno/vulkan/libvulkan_freedreno.so'
    command = ['ninja', '-C', 'build-android', '-j' + str(args.jobs),
               'src/freedreno/vulkan/libvulkan_freedreno.so']
    report = dict(baselineSha256=BASELINE, releaseSha256=RELEASE, patches=patches,
                  defaultMode='off', modes=['off', 'control', 'union'], restored=False,
                  gpuValidated=False)
    report['compilerExperiments'] = dict(coldCompile=args.cold_compile,
                                        allocations=args.compile_alloc,
                                        optimizationO3=args.compiler_o3)
    report['compilerExperiments']['profileGenerate'] = args.compiler_profile_generate
    report['compilerExperiments']['rangeFastpath'] = args.compiler_range_fastpath
    report['compilerExperiments']['ssaPoolAll'] = args.ssa_pool_all
    report['compilerExperiments']['deferredConstlen'] = args.deferred_constlen
    report['compilerExperiments']['thinLTO'] = args.compiler_thinlto
    report['compilerExperiments']['cpuTarget'] = args.compiler_cpu
    if args.compiler_profile_use:
        report['compilerExperiments']['profileUse'] = dict(
            path=str(profile_path), sha256=sha(profile_path))
    source_names = ('scripts/build_fdm_vrs.py', 'tests/turnip_vrs/tu_alyx_fdm_vrs.h',
                    'tests/turnip_vrs/tu_alyx_fdm_vrs.inc', 'tests/turnip_vrs/policy_test.cc')
    report['inputs'] = [dict(path=name, sha256=sha(repo / name)) for name in source_names]
    lock = (mesa / 'build-android/alyx-fdm-vrs.lock').open('w')
    fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)

    def build(label):
        with (out / (label + '.log')).open('w') as log:
            subprocess.run(command, cwd=mesa, stdout=log, stderr=subprocess.STDOUT, check=True)

    policy = out / 'policy-test'
    policy_command = ['g++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                      '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                      '-I' + str(repo / 'tests/turnip_vrs'),
                      str(repo / 'tests/turnip_vrs/policy_test.cc'), '-o', str(policy)]
    with (out / 'policy.log').open('w') as log:
        subprocess.run(policy_command, stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(policy)], stdout=log, stderr=subprocess.STDOUT, check=True)
    report['policyTests'] = dict(passed=True, sanitizers=['address', 'undefined'],
                               logSha256=sha(out / 'policy.log'))
    build('baseline')
    if sha(library) != BASELINE:
        raise RuntimeError('Configured source-sync baseline mismatch')
    originals = {p: p.read_bytes() if p.exists() else None for p in paths}
    for path, data in originals.items():
        if data is not None:
            backup = out / 'source-backup' / path.relative_to(mesa)
            backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(data)
    report['originalFiles'] = {str(p.relative_to(mesa)): None if data is None else
                              hashlib.sha256(data).hexdigest() for p, data in originals.items()}
    try:
        for item in patches[:len(release['patches'])]:
            subprocess.run(['git', 'apply', str(repo / item['path'])], cwd=mesa, check=True)
        build('release')
        report['rebuiltReleaseSha256'] = sha(library)
        if report['rebuiltReleaseSha256'] != RELEASE:
            raise RuntimeError('Rebuilt release mismatch; experiment not applied')
        for item in patches[len(release['patches']):]:
            subprocess.run(['git', 'apply', str(repo / item['path'])], cwd=mesa, check=True)
        for name in ('tu_alyx_fdm_vrs.h', 'tu_alyx_fdm_vrs.inc'):
            if (mesa / 'src/freedreno/vulkan' / name).read_bytes() != (repo / 'tests/turnip_vrs' / name).read_bytes():
                raise RuntimeError('Regenerate VRS patch after editing ' + name)
        if args.compile_alloc and (mesa / 'src/freedreno/ir3/ir3_alyx_use_pool.h').read_bytes() != (repo / 'tests/turnip_compile/ir3_use_pool.h').read_bytes():
            raise RuntimeError('Regenerate allocation patch after editing ir3_use_pool.h')
        build('candidate')
        candidate = out / 'vulkan.ad07xx.so'
        shutil.copyfile(library, candidate)
        report['candidateSha256'] = sha(candidate)
    finally:
        for path, data in originals.items():
            if data is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(data)
        try:
            build('restored')
            report['restoredSha256'] = sha(library)
            report['restored'] = report['restoredSha256'] == BASELINE
        finally:
            (out / 'build.json').write_text(json.dumps(report, indent=2) + '\n')
        if not report['restored']:
            raise RuntimeError('Restored baseline mismatch; see source-backup')
    print(json.dumps({key: report[key] for key in
                      ('rebuiltReleaseSha256', 'candidateSha256', 'restoredSha256')}, indent=2))


if __name__ == '__main__':
    main()
