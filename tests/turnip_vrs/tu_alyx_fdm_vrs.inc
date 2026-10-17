/* SPDX-License-Identifier: MIT
 * Included by tu_cmd_buffer.cc after the normal Turnip headers.
 * Experimental: primary, unsuspended, single-sample dynamic rendering only.
 */
#include "tu_alyx_fdm_vrs.h"
#include "util/log.h"
#include <atomic>
#include <mutex>

static std::mutex alyx_vrs_cache_mutex;
static alyx_vrs::MapCache alyx_vrs_cache;
static std::atomic<uint64_t> alyx_vrs_cache_hits{0}, alyx_vrs_cache_misses{0};
static bool alyx_vrs_cache_enabled()
{
   static const bool enabled = [] {
      const char *value = getenv("ALYX_FDM_VRS_CACHE");
      return !value || strcmp(value, "0");
   }();
   return enabled;
}

template <chip CHIP>
static void
tu_alyx_vrs_begin(struct tu_cmd_buffer *cmd, const VkRenderingInfo *info)
{
   cmd->state.alyx_vrs = {};
   if (CHIP != A7XX || alyx_vrs::mode() == alyx_vrs::Off ||
       !cmd->device->physical_device->info->props.has_attachment_shading_rate ||
       cmd->vk.level != VK_COMMAND_BUFFER_LEVEL_PRIMARY ||
       info->flags != 0 || cmd->state.per_layer_render_area ||
       cmd->state.suspend_resume != SR_NONE ||
       !cmd->dynamic_pass.has_fdm || cmd->dynamic_pass.has_msrtss ||
       cmd->dynamic_pass.subpass_count != 1 ||
       cmd->state.subpass->samples != VK_SAMPLE_COUNT_1_BIT ||
       cmd->state.subpass->custom_resolve ||
       cmd->state.subpass->fsr_attachment != VK_ATTACHMENT_UNUSED ||
       (info->viewMask != 0 && info->viewMask != 1 && info->viewMask != 3) ||
       (!info->viewMask && info->layerCount != 1))
      return;

   const unsigned layers = info->viewMask == 3 ? 2 : 1;
   const unsigned fdm_att = cmd->dynamic_pass.fragment_density_map.attachment;
   const struct tu_image_view *fdm = cmd->state.attachments[fdm_att];
   if (!fdm || !fdm->image->map || fdm->vk.format != VK_FORMAT_R8G8_UNORM ||
       fdm->image->layout[0].tile_mode != TILE6_LINEAR ||
       fdm->image->layout[0].cpp != 2 || fdm->vk.layer_count < layers ||
       !fdm->vk.extent.width || !fdm->vk.extent.height ||
       fdm->vk.extent.width > cmd->state.framebuffer->width ||
       fdm->vk.extent.height > cmd->state.framebuffer->height)
      return;
   /* The native sampler indexes the mapped image directly. Validate its entire
    * possible read range before choosing this backend, not after recording. */
   const uint64_t last = uint64_t(fdm->view.offset) +
      uint64_t(layers - 1) * fdm->view.layer_size +
      uint64_t(fdm->vk.extent.height - 1) * fdm->view.pitch +
      uint64_t(fdm->vk.extent.width) * 2;
   if (last > fdm->image->total_size)
      return;
   for (unsigned a = 0; a < cmd->dynamic_pass.attachment_count; a++) {
      const struct tu_image_view *view = cmd->state.attachments[a];
      if (view && (view->image->vk.create_flags & VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT))
         return;
   }
   alyx_vrs::Layout layout;
   if (!alyx_vrs::layout(cmd->state.framebuffer->width,
                         cmd->state.framebuffer->height, layers, &layout))
      return;
   struct tu_cs_memory memory;
   const VkResult result = tu_cs_alloc(&cmd->sub_cs,
      DIV_ROUND_UP(layout.bytes, 256), 64, &memory);
   if (result != VK_SUCCESS) {
      /* No pass mutation happened yet. Do not hide allocator errors. */
      vk_command_buffer_set_error(&cmd->vk, result);
      return;
   }
   cmd->state.alyx_vrs.source = fdm;
   cmd->state.alyx_vrs.map = (uint8_t *)memory.map;
   cmd->state.alyx_vrs.iova = memory.iova;
   cmd->state.alyx_vrs.layout = layout;
   memset(memory.map, 0, layout.bytes);

   /* Ordinary full-resolution GMEM/sysmem handling must replace tile FDM.
    * The source attachment remains alive in attachments[] until EndRendering. */
   cmd->dynamic_pass.has_fdm = false;
   cmd->dynamic_pass.has_layered_fdm = false;
   cmd->dynamic_pass.fragment_density_map.attachment = VK_ATTACHMENT_UNUSED;
   /* A previous FDM pass may have patched cached shader constants. Re-emit
    * identity constants for this pass even when its pipeline is unchanged. */
   cmd->state.dirty |= TU_CMD_DIRTY_FDM;
}

static void
tu_alyx_vrs_end(struct tu_cmd_buffer *cmd,
                const VkRenderPassFragmentDensityMapOffsetEndInfoEXT *info)
{
   if (!cmd->state.alyx_vrs.map)
      return;
   static_assert(MIN_FDM_TEXEL_SIZE >= 8, "VRS corner coverage requires >=8px source texels");
   const auto &state = cmd->state.alyx_vrs;
   const unsigned count = info ? info->fragmentDensityOffsetCount : 0;
   alyx_vrs::Offset offsets[2] = {};
   if (count && (count != state.layout.layers || !info->pFragmentDensityOffsets))
      return; /* Allocation was initialized to 1x1: malformed offsets fail closed. */
   for (unsigned i = 0; i < count; i++)
      offsets[i] = {info->pFragmentDensityOffsets[i].x, info->pFragmentDensityOffsets[i].y};
   const struct tu_image_view *fdm = state.source;
   const bool direct_rg = fdm->swizzle[0] == PIPE_SWIZZLE_X &&
                          fdm->swizzle[1] == PIPE_SWIZZLE_Y;
   const unsigned shift_x = CLAMP(util_logbase2_ceil(cmd->state.framebuffer->width / fdm->vk.extent.width),
                                 MIN_FDM_TEXEL_SIZE_LOG2, MAX_FDM_TEXEL_SIZE_LOG2);
   const unsigned shift_y = CLAMP(util_logbase2_ceil(cmd->state.framebuffer->height / fdm->vk.extent.height),
                                 MIN_FDM_TEXEL_SIZE_LOG2, MAX_FDM_TEXEL_SIZE_LOG2);
   alyx_vrs::CacheKey cache_key{};
   cache_key.width = cmd->state.framebuffer->width;
   cache_key.height = cmd->state.framebuffer->height;
   cache_key.source_width = fdm->vk.extent.width;
   cache_key.source_height = fdm->vk.extent.height;
   cache_key.layers = state.layout.layers;
   cache_key.shift_x = shift_x;
   cache_key.shift_y = shift_y;
   cache_key.mode = alyx_vrs::mode();
   for (unsigned i = 0; i < count; ++i) cache_key.offsets[i] = offsets[i];
   const alyx_vrs::CacheSource source = {
      (const uint8_t *)fdm->image->map + fdm->view.offset,
      fdm->image->total_size - fdm->view.offset, fdm->view.pitch, fdm->view.layer_size
   };
   const bool cacheable = direct_rg && alyx_vrs_cache_enabled() &&
      alyx_vrs::MapCache::valid(cache_key, source, state.layout);
   bool hit = false;
   if (cacheable) {
      std::lock_guard<std::mutex> lock(alyx_vrs_cache_mutex);
      hit = alyx_vrs_cache.lookup(cache_key, source, state.layout, state.map);
   }
   if (hit) ++alyx_vrs_cache_hits;
   else {
      if (cacheable) ++alyx_vrs_cache_misses;
      alyx_vrs::fill(state.map, state.layout,
      cmd->state.framebuffer->width, cmd->state.framebuffer->height,
      alyx_vrs::mode(), count ? offsets : NULL, count,
      [&](int32_t x, int32_t y, unsigned layer) {
         if (direct_rg) {
            const unsigned i = alyx_vrs::source_coord(x, fdm->vk.extent.width, shift_x);
            const unsigned j = alyx_vrs::source_coord(y, fdm->vk.extent.height, shift_y);
            const uint8_t *pixel = (const uint8_t *)fdm->image->map + fdm->view.offset +
               uint64_t(layer) * fdm->view.layer_size + uint64_t(j) * fdm->view.pitch + 2 * i;
            return alyx_vrs::density_rate(pixel[0], pixel[1]);
         }
         struct tu_frag_area area;
         tu_fragment_density_map_sample(state.source, x, y,
            cmd->state.framebuffer->width, cmd->state.framebuffer->height, layer, &area);
         return alyx_vrs::rate(area.width, area.height);
      });
      if (cacheable) {
         std::lock_guard<std::mutex> lock(alyx_vrs_cache_mutex);
         alyx_vrs_cache.store(cache_key, source, state.layout, state.map);
      }
   }
   /* Bounded evidence of actual accepted passes and generated rates. This is
    * deliberately not an FPS measurement or a claim that every draw uses VRS. */
   static std::atomic<uint64_t> passes{0};
   const uint64_t serial = passes.fetch_add(1, std::memory_order_relaxed) + 1;
   if (serial <= 8 || serial % 1024 == 0) {
      unsigned counts[16] = {};
      for (unsigned layer = 0; layer < state.layout.layers; layer++)
         for (unsigned y = 0; y < state.layout.height; y++)
            for (unsigned x = 0; x < state.layout.width; x++)
               counts[state.map[layer * state.layout.layer_size + y * state.layout.pitch + x] & 15]++;
      mesa_logi("ALYX_FDM_VRS pass=%llu mode=%s framebuffer=%ux%u source=%ux%u layers=%u rate1x1=%u rate2x2=%u rate4x4=%u cells=%u bytes=%u",
         (unsigned long long)serial, alyx_vrs::mode() == alyx_vrs::Union ? "union" : "control",
         cmd->state.framebuffer->width, cmd->state.framebuffer->height,
         state.source->vk.extent.width, state.source->vk.extent.height,
         state.layout.layers, counts[0], counts[5], counts[10],
         state.layout.width * state.layout.height * state.layout.layers, state.layout.bytes);
      size_t cache_bytes; unsigned entries;
      {
         std::lock_guard<std::mutex> lock(alyx_vrs_cache_mutex);
         cache_bytes = alyx_vrs_cache.bytes(); entries = alyx_vrs_cache.size();
      }
      mesa_logi("ALYX_FDM_VRS_CACHE enabled=%u hit=%u hits=%llu misses=%llu entries=%u bytes=%zu",
         unsigned(alyx_vrs_cache_enabled()), unsigned(hit),
         (unsigned long long)alyx_vrs_cache_hits.load(), (unsigned long long)alyx_vrs_cache_misses.load(),
         entries, cache_bytes);
   }
}
