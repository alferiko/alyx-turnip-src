"""Generate a local Meson patch for CPU compiler PGO, not shader optimization.

The builder applies this after its pinned release and restores Meson sources.
Only compiler/util static libraries are instrumented. The Turnip DSO links the
NDK profile runtime, which writes LLVM_PROFILE_FILE when the fixture exits.
"""
import difflib
from pathlib import Path


def make_patch(mesa: Path, profile: Path | None, thin_lto: bool = False,
               cpu: str | None = None) -> str:
    if cpu not in (None, 'cortex-a78c'):
        raise ValueError('Unsupported CPU target')
    if profile and any(c in str(profile) for c in "'\n\r\\"):
        raise ValueError('Profile path cannot be represented in Meson literal')
    flag = '-fprofile-use=' + str(profile) if profile else '-fprofile-generate'
    targets = [
        ('src/compiler/nir/meson.build', '_libnir = static_library('),
        ('src/freedreno/ir3/meson.build', 'libfreedreno_ir3 = static_library('),
        ('src/compiler/spirv/meson.build', 'libvtn = static_library('),
        ('src/util/meson.build', '_libmesa_util = static_library('),
    ]
    result = ''
    for name, marker in targets:
        old = (mesa / name).read_text()
        begin = old.index('  c_args : [', old.index(marker))
        end = old.index('],', begin)
        extra = ", '-O3', '" + flag + "'"
        if thin_lto:
            extra += ", '-flto=thin'"
        if cpu:
            extra += ", '-mcpu=" + cpu + "'"
        new = old[:end] + extra + old[end:]
        result += ''.join(difflib.unified_diff(
            old.splitlines(True), new.splitlines(True),
            fromfile='a/' + name, tofile='b/' + name))
    if not profile or thin_lto:
        name = 'src/freedreno/vulkan/meson.build'
        old = (mesa / name).read_text()
        begin = old.index('  link_args : [', old.index('libvulkan_freedreno ='))
        end = old.index('],', begin)
        extra = '' if profile else ", '-fprofile-generate'"
        if thin_lto:
            extra += ", '-flto=thin', '-Wl,--thinlto-jobs=8'"
        new = old[:end] + extra + old[end:]
        result += ''.join(difflib.unified_diff(
            old.splitlines(True), new.splitlines(True),
            fromfile='a/' + name, tofile='b/' + name))
    return result
