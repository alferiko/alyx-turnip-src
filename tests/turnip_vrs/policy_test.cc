/* SPDX-License-Identifier: MIT */
#include "tu_alyx_fdm_vrs.h"
#include <assert.h>
#include <limits>
#include <vector>
#include <stdio.h>
using namespace alyx_vrs;
static void cache_tests()
{
   MapCache cache;
   CacheKey key{};
   key.width=17;key.height=9;key.source_width=3;key.source_height=2;
   key.layers=2;key.shift_x=3;key.shift_y=3;key.mode=Union;
   Layout l{};assert(layout(key.width,key.height,key.layers,&l));
   std::vector<uint8_t> source(80,255), output(l.bytes,5), restored(l.bytes,0xcd);
   CacheSource src{source.data(),source.size(),16,40};
   assert(!cache.lookup(key,src,l,restored.data()));
   assert(cache.store(key,src,l,output.data()));
   assert(cache.lookup(key,src,l,restored.data()) && restored==output);
   assert(cache.size()==1 && cache.bytes()==24+l.bytes);
   // Padding and host address do not affect the sampled RG8 content.
   source[7]=19;source[39]=13;
   auto relocated=source;CacheSource other{relocated.data(),relocated.size(),16,40};
   assert(cache.lookup(key,other,l,restored.data()));
   // Mutation at the same address, including the second eye, must miss.
   for(unsigned i: {0u,5u,16u,40u,61u}) {
      source[i]=127;assert(!cache.lookup(key,src,l,restored.data()));source[i]=255;
   }
   CacheKey changed=key;changed.offsets[1].x=1;assert(!cache.lookup(changed,src,l,restored.data()));
   changed=key;changed.shift_x=4;assert(!cache.lookup(changed,src,l,restored.data()));
   changed=key;changed.shift_y=4;assert(!cache.lookup(changed,src,l,restored.data()));
   changed=key;changed.mode=Control;assert(!cache.lookup(changed,src,l,restored.data()));
   changed=key;changed.width=18;assert(!cache.lookup(changed,src,l,restored.data()));
   changed=key;changed.source_width=2;assert(!cache.lookup(changed,src,l,restored.data()));
   other=src;other.bytes=61;assert(!cache.lookup(key,other,l,restored.data()));
   other=src;other.layer_stride=8;assert(!cache.store(key,other,l,output.data()));
   other=src;other.pitch=5;assert(!cache.store(key,other,l,output.data()));
   other=src;other.data=nullptr;assert(!cache.store(key,other,l,output.data()));
   Layout invalid=l;invalid.bytes++;assert(!cache.store(key,src,invalid,output.data()));
   // Store owns independent copies, and LRU evicts the oldest unused entry.
   for(unsigned i=1;i<8;i++){changed=key;changed.offsets[0].x=i;assert(cache.store(changed,src,l,output.data()));}
   assert(cache.lookup(key,src,l,restored.data()));
   changed=key;changed.offsets[0].x=8;assert(cache.store(changed,src,l,output.data()));
   assert(cache.size()==8 && cache.lookup(key,src,l,restored.data()));
   changed=key;changed.offsets[0].x=1;assert(!cache.lookup(changed,src,l,restored.data()));
   const size_t prior=cache.bytes();assert(cache.store(key,src,l,output.data()));assert(cache.bytes()==prior);
   cache.clear();assert(cache.size()==0&&cache.bytes()==0);
   key.width=key.height=4096;key.source_width=key.source_height=512;
   assert(layout(4096,4096,2,&l));
   source.assign(512*512*4,63);output.assign(l.bytes,10);restored.resize(l.bytes);
   src={source.data(),source.size(),1024,512*1024};
   assert(cache.store(key,src,l,output.data()));
   changed=key;changed.offsets[0].x=1;assert(cache.store(changed,src,l,output.data()));
   assert(cache.size()==1 && cache.bytes()<=MapCache::Budget);
   assert(!cache.lookup(key,src,l,restored.data()));
   assert(cache.lookup(changed,src,l,restored.data())&&restored==output);
   puts("VRS exact-content cache: invalidation, padding, relocation, LRU and 2MiB cap passed");
}
int main()
{
   cache_tests();
   assert(parse_mode(nullptr) == Off && parse_mode("1") == Off);
   assert(parse_mode("union") == Union && parse_mode("control") == Control);
   assert(encode(rate(1, 1)) == 0 && encode(rate(2, 2)) == 5 && encode(rate(4, 4)) == 10);
   assert(encode(rate(3.99f, 1.99f)) == 4);
   assert(encode(rate(std::numeric_limits<float>::infinity(), NAN)) == 0);
   assert(encode(rate(-1, 0)) == 0);
   /* UNORM reciprocal rounding must not request a coarser footprint. */
   assert(encode(rate(255.f / 128, 255.f / 64)) == 1);
   assert(encode(rate(255.f / 127, 255.f / 63)) == 6);
   for(unsigned a=0;a<256;a++) for(unsigned b=0;b<256;b++)
      assert(encode(density_rate(a,b)) == encode(rate(a ? 255.f/a : INFINITY,b ? 255.f/b : INFINITY)));
   assert(source_coord(INT32_MIN,90,5)==0 && source_coord(-1,90,5)==0);
   assert(source_coord(31,90,5)==0 && source_coord(32,90,5)==1);
   assert(source_coord(INT32_MAX,90,5)==89);
   Layout l{};
   assert(!layout(0, 1, 1, &l) && !layout(UINT32_MAX, 1, 1, &l));
   assert(!layout(4097, 1, 1, &l) && !layout(8, 8, 3, &l));
   assert(layout(17, 9, 2, &l));
   assert(l.width == 3 && l.height == 2 && l.pitch == 64 && l.bytes == 256);
   std::vector<uint8_t> map(l.bytes + 2, 0xcd);
   auto coarse = [](int32_t, int32_t, unsigned) { return rate(4, 4); };
   fill(map.data()+1, l, 17, 9, Control, nullptr, 0, coarse);
   for (unsigned i=1;i<=l.bytes;i++) assert(map[i] == 0);
   fill(map.data()+1, l, 17, 9, Union, nullptr, 0, coarse);
   assert(map[1] == 10 && map[3] == 10 && map[4] == 0);
   assert(map.front() == 0xcd && map.back() == 0xcd);
   assert(!memcmp(map.data()+1, map.data()+1+l.layer_size, l.layer_size));
   auto eye_center = [](int32_t x, int32_t y, unsigned eye) {
      return eye == 1 && x >= 8 && y < 8 ? rate(1,1) : rate(4,4);
   };
   fill(map.data()+1,l,17,9,Union,nullptr,0,eye_center);
   assert(map[1] == 10 && map[2] == 0 && map[1+l.layer_size+1] == 0);
   Offset offsets[2] = {{1,0},{1,0}};
   /* One corner crosses a source cell edge: a center-only sample would miss it. */
   auto edge = [](int32_t x, int32_t, unsigned) { return x < 0 ? rate(1,1) : rate(4,4); };
   fill(map.data()+1,l,17,9,Union,offsets,2,edge);
   assert(map[1] == 0 && map[2] == 10);
   fill(map.data()+1,l,17,9,Union,offsets,1,coarse);
   for (unsigned i=1;i<=l.bytes;i++) assert(map[i] == 0);
   assert(subtract_offset(4095,INT32_MIN) == INT32_MAX);
   /* Exhaustive anisotropic union and conservative quantization boundaries. */
   for (unsigned a=1;a<=16;a++) for(unsigned b=1;b<=16;b++) {
      Rate r = intersect(rate(a/4.f,b/4.f),rate(b/4.f,a/4.f));
      assert((1u<<r.x) <= (a/4.f < 1 ? 1 : a/4.f));
      assert((1u<<r.y) <= (b/4.f < 1 ? 1 : b/4.f));
   }
   assert(layout(4096,4096,2,&l));
   assert(l.width==512 && l.height==512 && l.bytes==524288);
   assert((l.pitch%64)==0 && (l.layer_size%64)==0);
   /* An independent pixel oracle verifies every source texel intersecting a
    * rate cell, including different eye offsets and clipped framebuffer edges.
    * Source cells are 32px, as in the actual direct-FDM attachment. */
   unsigned checks=0;
   for (unsigned iteration=0; iteration<128; iteration++) {
      const unsigned w=1+(iteration*37)%259, h=1+(iteration*19)%133;
      assert(layout(w,h,2,&l));
      std::vector<uint8_t> actual(l.bytes,0xcd);
      Offset os[2]={{int32_t(iteration%65)-32, int32_t(iteration%43)-21},
                    {int32_t(iteration%71)-35, int32_t(iteration%53)-26}};
      auto source=[](int32_t x,int32_t y,unsigned eye) {
         const unsigned tx=unsigned(x<0?0:x)/32, ty=unsigned(y<0?0:y)/32;
         return Rate{(tx+ty+eye)%3,(tx*3+ty+eye*2)%3};
      };
      fill(actual.data(),l,w,h,Union,os,2,source);
      for (unsigned my=0;my<l.height;my++) for (unsigned mx=0;mx<l.width;mx++) {
         Rate expected{2,2};
         for (unsigned y=my*8;y<h && y<my*8+8;y++)
            for (unsigned x=mx*8;x<w && x<mx*8+8;x++)
               for (unsigned eye=0;eye<2;eye++)
                  expected=intersect(expected,source(int32_t(x)-os[eye].x,int32_t(y)-os[eye].y,eye));
         assert(actual[my*l.pitch+mx]==encode(expected));
         assert(actual[l.layer_size+my*l.pitch+mx]==encode(expected));
         checks++;
      }
   }
   printf("VRS pixel-oracle cells: %u\n",checks);
   puts("VRS policy: all checks passed");
}
