/* SPDX-License-Identifier: MIT */
// Compile against the reconstructed msaa-vrs-cache header, not the base header.
#include <tu_alyx_fdm_vrs.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

int main()
{
    using namespace alyx_vrs;
    // Explicit hardware-policy examples, including preserving a sharp center.
    uint8_t examples[] = {0, 5, 10, 8, 6, 9};
    const uint8_t expected[] = {0, 5, 9, 4, 5, 9};
    clamp_msaa2(examples, sizeof(examples));
    assert(!memcmp(examples, expected, sizeof(expected)));

    CacheKey single{};
    single.width = single.height = 16;
    single.source_width = single.source_height = 1;
    single.layers = 1;
    single.shift_x = single.shift_y = 5;
    single.mode = Union;
    single.samples = 1;
    CacheKey msaa = single;
    msaa.samples = 2;
    Layout layout_value{};
    assert(layout(16, 16, 1, &layout_value));
    uint8_t density[] = {63, 63};
    CacheSource source{density, sizeof(density), 2, 2};
    std::vector<uint8_t> full(layout_value.bytes, 10);
    full[0] = 0;
    auto capped = full;
    clamp_msaa2(capped.data(), capped.size());
    assert(capped[0] == 0 && capped[1] == 9 && full[1] == 10);
    MapCache cache;
    std::vector<uint8_t> output(layout_value.bytes, 0xcd);
    assert(cache.store(single, source, layout_value, full.data()));
    assert(!cache.lookup(msaa, source, layout_value, output.data()));
    assert(cache.store(msaa, source, layout_value, capped.data()));
    std::fill(full.begin(), full.end(), 0xff);
    std::fill(capped.begin(), capped.end(), 0xff);
    // Cache must own both maps and return the final MSAA map without clamping.
    for (unsigned i = 0; i < 20; ++i) {
        assert(cache.lookup(msaa, source, layout_value, output.data()));
        assert(output[0] == 0 && output[1] == 9);
        assert(cache.lookup(single, source, layout_value, output.data()));
        assert(output[0] == 0 && output[1] == 10);
    }
    assert(cache.size() == 2);
    for (unsigned samples : {0u, 4u, 8u}) {
        auto unsupported = single;
        unsupported.samples = samples;
        assert(!cache.store(unsupported, source, layout_value, output.data()));
        assert(!cache.lookup(unsupported, source, layout_value, output.data()));
    }
    density[0] = 255;
    assert(!cache.lookup(single, source, layout_value, output.data()));
    assert(!cache.lookup(msaa, source, layout_value, output.data()));
    puts("PASS: MSAA rate limits, sharp center, final-map ownership, sample-count isolation and invalidation");
}
