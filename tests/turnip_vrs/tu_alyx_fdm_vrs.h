/* SPDX-License-Identifier: MIT
 * Experimental CPU-side FDM -> A7xx quality-buffer policy. No Mesa dependency.
 */
#ifndef TU_ALYX_FDM_VRS_H
#define TU_ALYX_FDM_VRS_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

namespace alyx_vrs {
enum Mode { Off, Control, Union };
inline Mode parse_mode(const char *value)
{
   if (value && !strcmp(value, "control")) return Control;
   if (value && !strcmp(value, "union")) return Union;
   return Off;
}
inline Mode mode()
{
   /* Read once: changing pipeline-state policy halfway through a process is unsafe. */
   static const Mode selected = parse_mode(getenv("ALYX_FDM_VRS"));
   return selected;
}
struct Layout { uint32_t width, height, pitch, layer_size, layers, bytes; };
inline bool layout(uint32_t width, uint32_t height, uint32_t layers, Layout *out)
{
   /* Bounded arithmetic, CPU work, and suballocator growth. */
   if (!width || !height || width > 4096 || height > 4096 || !layers || layers > 2)
      return false;
   out->width = (width + 7) / 8;
   out->height = (height + 7) / 8;
   out->pitch = (out->width + 63) & ~63u;
   out->layer_size = out->pitch * out->height;
   out->layers = layers;
   out->bytes = out->layer_size * layers;
   return true;
}
inline unsigned exponent(float area)
{
   /* Never round toward a coarser rate. Invalid / zero density means full quality. */
   if (!isfinite(area) || area < 2.0f) return 0;
   return area < 4.0f ? 1 : 2;
}
struct Rate { unsigned x, y; };
inline Rate rate(float x, float y) { return { exponent(x), exponent(y) }; }
/* Exact equivalent of rate(255 / x, 255 / y) for RG8 UNORM, avoiding format
 * dispatch and reciprocal operations in the per-cell sampling loop. */
inline unsigned density_exponent(uint8_t density)
{
   return !density || density >= 128 ? 0 : density >= 64 ? 1 : 2;
}
inline Rate density_rate(uint8_t x, uint8_t y)
{
   return {density_exponent(x), density_exponent(y)};
}
inline unsigned source_coord(int32_t pixel, unsigned extent, unsigned shift)
{
   const unsigned cell = pixel <= 0 ? 0 : uint32_t(pixel) >> shift;
   return cell < extent ? cell : extent - 1;
}
inline Rate intersect(Rate a, Rate b)
{
   return { a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y };
}
inline uint8_t encode(Rate r) { return uint8_t((r.x << 2) | r.y); }
struct Offset { int32_t x, y; };
/* CPU-only, exact-content LRU. The caller serializes access. No image handles,
 * mappings or command-buffer allocations survive a lookup. A hit still copies
 * into the current command buffer's own allocation. */
struct CacheKey {
   uint32_t width, height, source_width, source_height, layers, shift_x, shift_y, mode;
   Offset offsets[2];
};
static_assert(sizeof(CacheKey) == 12 * sizeof(uint32_t), "canonical cache key");
struct CacheSource {
   const uint8_t *data;
   uint64_t bytes, pitch, layer_stride;
};
class MapCache {
public:
   static constexpr unsigned Capacity = 8;
   static constexpr size_t Budget = 2 * 1024 * 1024;
   MapCache() = default;
   MapCache(const MapCache &) = delete;
   MapCache &operator=(const MapCache &) = delete;
   ~MapCache() { clear(); }
   size_t bytes() const { return used; }
   unsigned size() const {
      unsigned n = 0; for (const auto &entry : entries) n += entry.data != nullptr;
      return n;
   }
   void clear() { for (auto &entry : entries) erase(entry); }
   static bool valid(const CacheKey &key, const CacheSource &source, const Layout &l) {
      Layout expected{};
      if (!source.data || !key.source_width || key.source_width > 512 ||
          !key.source_height || key.source_height > 512 || key.shift_x > 31 || key.shift_y > 31 ||
          (key.mode != Control && key.mode != Union) ||
          !layout(key.width, key.height, key.layers, &expected) || memcmp(&l, &expected, sizeof(l)))
         return false;
      const uint64_t row = uint64_t(key.source_width) * 2;
      if (source.pitch < row || source.bytes < row ||
          key.source_height - 1 > (source.bytes - row) / source.pitch)
         return false;
      const uint64_t plane = uint64_t(key.source_height - 1) * source.pitch + row;
      return key.layers == 1 || (source.layer_stride >= plane && source.layer_stride <= source.bytes - plane);
   }
   bool lookup(const CacheKey &key, const CacheSource &source, const Layout &l, uint8_t *out) {
      if (!out || !valid(key, source, l)) return false;
      for (auto &entry : entries) {
         if (!entry.data || memcmp(&entry.key, &key, sizeof(key))) continue;
         if (!same_source(entry.data, key, source)) continue;
         memcpy(out, entry.data + input_size(key), l.bytes);
         entry.age = ++clock;
         return true;
      }
      return false;
   }
   bool store(const CacheKey &key, const CacheSource &source, const Layout &l, const uint8_t *map) {
      if (!map || !valid(key, source, l)) return false;
      const size_t input = input_size(key), total = input + l.bytes;
      if (total > Budget) return false;
      // Avoid duplicate publication when two recording threads missed together.
      for (auto &entry : entries)
         if (entry.data && !memcmp(&entry.key, &key, sizeof(key)) && same_source(entry.data, key, source)) {
            entry.age = ++clock; return true;
         }
      while (used + total > Budget || size() == Capacity) erase(oldest());
      auto *copy = static_cast<uint8_t *>(malloc(total));
      if (!copy) return false; // Cache allocation failure never fails rendering.
      const size_t row = size_t(key.source_width) * 2;
      size_t cursor = 0;
      for (unsigned layer = 0; layer < key.layers; ++layer)
         for (unsigned y = 0; y < key.source_height; ++y, cursor += row)
            memcpy(copy + cursor, source.data + layer * source.layer_stride + y * source.pitch, row);
      memcpy(copy + input, map, l.bytes);
      for (auto &entry : entries) if (!entry.data) {
         entry.key = key; entry.data = copy; entry.bytes = total; entry.age = ++clock;
         used += total; return true;
      }
      free(copy); return false;
   }
private:
   struct Entry { CacheKey key{}; uint8_t *data = nullptr; size_t bytes = 0; uint64_t age = 0; };
   Entry entries[Capacity]{};
   size_t used = 0;
   uint64_t clock = 0;
   static size_t input_size(const CacheKey &key) { return size_t(key.source_width) * key.source_height * key.layers * 2; }
   static bool same_source(const uint8_t *saved, const CacheKey &key, const CacheSource &source) {
      const size_t row = size_t(key.source_width) * 2;
      for (unsigned layer = 0; layer < key.layers; ++layer)
         for (unsigned y = 0; y < key.source_height; ++y, saved += row)
            if (memcmp(saved, source.data + layer * source.layer_stride + y * source.pitch, row)) return false;
      return true;
   }
   Entry &oldest() {
      Entry *result = nullptr;
      for (auto &entry : entries) if (entry.data && (!result || entry.age < result->age)) result = &entry;
      return *result;
   }
   void erase(Entry &entry) { if (entry.data) { used -= entry.bytes; free(entry.data); entry = {}; } }
};
inline int32_t subtract_offset(uint32_t value, int32_t offset)
{
   const int64_t result = int64_t(value) - offset;
   return result < INT32_MIN ? INT32_MIN : result > INT32_MAX ? INT32_MAX : int32_t(result);
}

/* Source texels must cover >=8 framebuffer pixels in each axis. The four
 * corners then visit every source cell overlapping an 8x8 quality-buffer cell,
 * including offset-straddling cells. Each eye can independently demand 1x1.
 * Identical conservative maps are emitted for both layers. Unused pitch bytes
 * remain 1x1 so padding can never inject a coarser rate.
 */
template <typename Sample>
inline void fill(uint8_t *dst, const Layout &l, uint32_t fb_width, uint32_t fb_height,
                 Mode selected, const Offset *offsets, uint32_t offset_count, Sample sample)
{
   memset(dst, 0, l.bytes);
   if (selected != Union || (offsets && offset_count != l.layers) ||
       (!offsets && offset_count)) return;
   for (uint32_t y = 0; y < l.height; y++) {
      for (uint32_t x = 0; x < l.width; x++) {
         const uint32_t x0 = x * 8, y0 = y * 8;
         const uint32_t x1 = x0 + 7 < fb_width ? x0 + 7 : fb_width - 1;
         const uint32_t y1 = y0 + 7 < fb_height ? y0 + 7 : fb_height - 1;
         Rate shared = { 2, 2 };
         for (uint32_t eye = 0; eye < l.layers; eye++) {
            const Offset offset = offsets ? offsets[eye] : Offset{};
            const uint32_t xs[] = {x0, x1}, ys[] = {y0, y1};
            for (unsigned j = 0; j < 2; j++)
               for (unsigned i = 0; i < 2; i++)
                  shared = intersect(shared, sample(subtract_offset(xs[i], offset.x),
                     subtract_offset(ys[j], offset.y), eye));
         }
         for (uint32_t eye = 0; eye < l.layers; eye++)
            dst[eye * l.layer_size + y * l.pitch + x] = encode(shared);
      }
   }
}
} /* namespace alyx_vrs */
#endif
