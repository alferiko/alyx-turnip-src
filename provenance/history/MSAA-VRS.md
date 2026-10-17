# Quest MSAA2 + hardware VRS

Driver SHA256: `d7cfb0b4df979c105a305a849ae9c6f6fd2af1a82f067317a7eec050a26d1764`.
Pipeline-cache UUID: `00feb00a67095f51d19b280f8fdecd76`.

This is the pinned combined compiler plus `tests/turnip_vrs/msaa2-vrs.patch`.
The compiler/PGO changes are unchanged. `scripts/build_msaa_vrs.py` verifies
every parent input, rebuilds the candidate, and restores the configured Mesa
sources and original baseline binary in `finally`.

The A7xx backend accepts ordinary MSAA2 dynamic rendering, including attachment
loads/stores and color/depth resolves. It reads the explicit immutable density
map even when Mesa's tile-FDM backend rejects multisample load/store. It retains
full-size multisample attachments and uses hardware shading rates. MSRTSS,
custom resolves, secondary command buffers and suspended passes stay excluded.

For MSAA2 the per-command map is capped to supported rates (at most 4x2). The
cache holds the original map, so MSAA clamping cannot corrupt a later
single-sample 4x4 pass. Driver mode `off` is unchanged.

The launcher hides Maximum with MSAA2 and selects Very high when necessary.
Launch and shader preparation also normalize persisted settings. The new
`release/pipeline-library/inputs-msaa-vrs.json` pins the measured driver/cache
identity; old-driver cache banks remain on disk and cannot be mistaken for new
ones. The recipe adapter now prepares MSAA2 FDM-capable pipelines as well.

Device evidence is in `staff/build/quest-msaa-vrs-20261011` in the parent
workspace. GPU readback checks HDR+D24S8, MSAA2 color/depth resolves, native Quest
eye regions, 2x2/4x2 shading, sharp centers and ordinary-pass restoration.
The real 2880x1536 game map matches the tested map byte for byte. Single-sample
4x4 regression and forced sysmem were also checked.

The APK produced for this change targets Quest. Universal/PICO builds retain
their old driver layout pin and require a separate compatibility review before
using this binary. This directory's historical `gpuValidated` field is not
used as a substitute for the separate device acceptance evidence.
