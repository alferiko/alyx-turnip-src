# Mesa upstream backports

Applied after the complete 23-patch Quest release bundle. Stages are cumulative.

1. `01-viewport-scissor.patch`: upstream 99b98992ca41, removes duplicate dynamic viewport/scissor command allocation/emission. Original diff, no functional adaptation.
   https://gitlab.freedesktop.org/mesa/mesa/-/commit/99b98992ca41

2. `02-gmem-fences.patch`: full MR44348 SUBPASS_FENCE series:
   `5efaa4a7225e`, `b280f8885ff6`, `831f2cd8f41e`, `10b1ef466fae`,
   `cc1cf17c2e0f`, `99200197cc36`.
   https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/44348
   Prerequisites: `df87847722c0` (64-bit framebuffer stage mask),
   `3a35704eaa86` (query-copy write flush), `2b6602eb4b20` (separate gen7/gen8
   event tables), `33643dab0650` (Fence event definition).
   Adapted to the older `gmem_disable_reason` field and existing device-property
   layout; query-copy calls the templated access-flush helper. The resolve-clean
   property stays disabled on Quest 3's gen2 GPU; gen3 behavior is not imposed
   on a740. Both GMEM and SYSMEM fallbacks remain present.

3. `03-descriptors-budget.patch`: original upstream diffs `ce90e3f3b4f2`
   (free failed descriptor allocations on pool fragmentation), `ea735758c4d8`
   (one atomic heap-usage snapshot for budget/usage), `59bb32c15b3e`
   (descriptor-update templates own immutable sampler data instead of retaining
   dangling layout pointers). No semantic adaptation.

4. `04-ir3-nir.patch`: IR3 constant reuse `516c078b1d90`, with prerequisites
   `dcc86869c7e1`, `c70c52159999`, `8664e26e1bb0`, `64bdcea0fad2`
   (source-modifier handling, denorm behavior and correct 16-bit SABS).
   Enables `nir_opt_fp_math_ctrl` (`36a13175fc65`), `nir_opt_phi_to_bool`
   (`5c57ee48316e`) and `nir_opt_algebraic_distribute_src_mods` (`f71d93287750`).
   The pass implementations already exist in the pinned Mesa base. All eight
   original diffs apply without semantic adaptations or a wholesale Mesa update.

The portable `scripts/build.py` applies these four backport stages as part of
each variant's ordered manifest. Patch and driver hashes remain pinned; building
a driver does not install it.
