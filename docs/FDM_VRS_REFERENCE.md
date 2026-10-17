# Experimental FDM to hardware VRS backend

Historical reference for the `compat` variant. For MSAA2 and final-map caching,
see [current variants](VARIANTS.md) and the [portable build guide](BUILD.md).
Историческое описание `compat`; MSAA2 и новый кэш описаны в документах выше.

This OFF-by-default experiment retains the 27-patch release driver chain
and adds `experimental-fdm-vrs.patch` afterward.

`ALYX_FDM_VRS` is read once, before pipelines are created:

| Value | Behavior in eligible passes |
| --- | --- |
| absent / `off` / unknown | Existing release FDM behavior |
| `control` | Hardware quality buffer with every cell set to 1x1 |
| `union` | Conservative density conversion and union of sharp regions across layers |

The map stores one byte per 8x8 framebuffer cell, with dimensions rounded up and
64-byte row/array alignment. Fragment dimensions are discretized to 1, 2, or 4;
conversion never chooses a coarser rate than the requested density. In particular,
UNORM 128/255 is slightly denser than 1/2 and remains 1x; 64/255 remains 2x.
The optional host adapter uses 127/255 and 63/255 for the 2x and 4x rings.
Four corner samples per eye cover every overlapping source texel because the
driver's minimum source texel footprint is 32 pixels. The same conservative
map is written for both layers.

Eligibility is intentionally narrow: A7xx with attachment shading rate support,
primary command buffers, unsuspended dynamic rendering, one subpass, 1x MSAA,
no MSRTSS/custom resolve/native FSR attachment/subsampled image/per-layer render
area, at most 4096x4096, and a mapped linear RG8 density view with a verified
read range. Supported view masks are 0, 1, and 3; non-multiview requires one
framebuffer layer. Everything else retains the previous driver path. The main
2880x1536 packed-stereo pass with a 90x48 single-layer map fits this contract.
Mesa already forces FDM images to linear hardware layout and maps their BO,
including when the application requests `VK_IMAGE_TILING_OPTIMAL`.

Each accepted pass gets separate command-buffer-owned map storage. Source maps
must be immutable and initialized before recording, as in the direct-FDM test
adapter. Sampling occurs at EndRendering and accounts for density offsets.
The backend removes software FDM scaling, emits identity shader constants, and
invalidates reused constants between passes. Shaders that read/write native
shading rate retain full density. `control` tests the replacement path without
coarse shading. Malformed density offsets leave the preinitialized 1x1 map.

The RG8 fast path now has an exact-content CPU LRU (eight entries, 2 MiB payload
budget shared across recording threads). Keys include framebuffer/source size,
layers, source-coordinate shifts, mode and offsets; all active input bytes must
also match. Padding, mapped addresses and GPU handles are not keys. Hits copy
cached bytes into the current command buffer's allocation. Misses use the same
conversion, and cache allocation failure only skips storage. Set the read-once
`ALYX_FDM_VRS_CACHE=0` for an otherwise identical uncached control. Bounded
`ALYX_FDM_VRS_CACHE` logs report hits, misses, entries and retained byte count.
The optional host's mode 3 uses a conservative projection-derived quality map;
it still selects the driver's existing `union` conversion.

The first eight accepted passes and every 1024th thereafter emit
`ALYX_FDM_VRS` through Mesa logging: mode, framebuffer/source dimensions, layers,
counts of 1x1/2x2/4x4 cells, and allocation size. These prove map generation, not
GPU execution or a performance gain. No log is emitted while the feature is off.

## Reproduction

Use the configured WSL Ubuntu-24.04 tree and its Meson virtual environment:

```sh
env PATH=/home/zer0k/alyx-xemu-source-20260926/venv/bin:/usr/bin:/bin \
  python3 scripts/build_fdm_vrs.py \
  --mesa /home/zer0k/alyx-xemu-source-20260926/mesa \
  --out /path/to/new-output-directory --jobs 12
```

The builder checks all release patch hashes, runs the CPU policy tests under
ASan/UBSan, rebuilds and matches the release driver, builds the candidate, and
restores/rebuilds the baseline even if the experiment fails. Logs, a source
backup, input hashes, and a build manifest are retained. Its lock prevents two
instances of this builder from mutating the same Mesa tree concurrently.

`policy_test.cc` uses an independent exhaustive per-pixel oracle over 19,384
rate cells and tests different eye offsets, clipped dimensions, conservative
UNORM boundaries, padding, maximum sizes, invalid density, and malformed offset
counts. The tests validate CPU map policy, not Vulkan conformance or GPU output.
They also cover exact-content invalidation, padding, allocation relocation, LRU
eviction and the cache memory budget. Hardware evidence and the projection
policy are described in `provenance/optimization-mk2-vrs-cache-projection.md`.

`generate_patch.py --base <release-stage4-source-snapshot>` regenerates the
patch from the tracked header/include files and exact-context source changes.
The build rejects a stale patch/header pair.
