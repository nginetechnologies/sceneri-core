//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#include "3rdparty/ozz/animation/runtime/sampling_job.h"

#include <Common/Assert/Assert.h>
#include <Common/Memory/Copy.h>

#include "3rdparty/ozz/animation/runtime/animation.h"
#include "3rdparty/ozz/base/maths/math_constant.h"
#include "3rdparty/ozz/base/maths/math_ex.h"
#include "3rdparty/ozz/base/maths/soa_transform.h"
#include "3rdparty/ozz/base/memory/allocator.h"

// Internal include file
#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.
#include "3rdparty/ozz/animation/runtime/animation_keyframe.h"

namespace ozz {
namespace animation {

namespace internal {
struct InterpSoaFloat3 {
  math::SimdFloat4 ratio[2];
  math::SoaFloat3 value[2];
};
struct InterpSoaQuaternion {
  math::SimdFloat4 ratio[2];
  math::SoaQuaternion value[2];
};
}  // namespace internal

bool SamplingJob::Validate() const {
  // Don't need any early out, as jobs are valid in most of the performance
  // critical cases.
  // Tests are written in multiple lines in order to avoid branches.
  bool valid = true;

  // Test for nullptr pointers.
  if (!animation || !cache) {
    return false;
  }
  valid &= !output.empty();

  const int num_soa_tracks = animation->num_soa_tracks();
  valid &= output.size() >= static_cast<size_t>(num_soa_tracks);

  // Tests cache size.
  valid &= cache->max_soa_tracks() >= num_soa_tracks;

  return valid;
}

namespace {
// Loops through the sorted key frames and update cache structure.
template <typename _Key>
void UpdateCacheCursor(float _ratio, int _num_soa_tracks,
                       const ozz::span<const _Key>& _keys, int* _cursor,
                       int* _cache, unsigned char* _outdated) {
    Assert(_num_soa_tracks >= 1);
  const int num_tracks = _num_soa_tracks * 4;
    Assert(_keys.begin() + num_tracks * 2 <= _keys.end());

  const _Key* cursor = nullptr;
  if (!*_cursor) {
    // Initializes interpolated entries with the first 2 sets of key frames.
    // The sorting algorithm ensures that the first 2 key frames of a track
    // are consecutive.
    for (int i = 0; i < _num_soa_tracks; ++i) {
      const int in_index0 = i * 4;                   // * soa size
      const int in_index1 = in_index0 + num_tracks;  // 2nd row.
      const int out_index = i * 4 * 2;
      _cache[out_index + 0] = in_index0 + 0;
      _cache[out_index + 1] = in_index1 + 0;
      _cache[out_index + 2] = in_index0 + 1;
      _cache[out_index + 3] = in_index1 + 1;
      _cache[out_index + 4] = in_index0 + 2;
      _cache[out_index + 5] = in_index1 + 2;
      _cache[out_index + 6] = in_index0 + 3;
      _cache[out_index + 7] = in_index1 + 3;
    }
    cursor = _keys.begin() + num_tracks * 2;  // New cursor position.

    // All entries are outdated. It cares to only flag valid soa entries as
    // this is the exit condition of other algorithms.
    const int num_outdated_flags = (_num_soa_tracks + 7) / 8;
    for (int i = 0; i < num_outdated_flags - 1; ++i) {
      _outdated[i] = 0xff;
    }
    _outdated[num_outdated_flags - 1] =
        0xff >> (num_outdated_flags * 8 - _num_soa_tracks);
  } else {
    cursor = _keys.begin() + *_cursor;  // Might be == end()
      Assert(cursor >= _keys.begin() + num_tracks * 2 && cursor <= _keys.end());
  }

  // Search for the keys that matches _ratio.
  // Iterates while the cache is not updated with left and right keys required
  // for interpolation at time ratio _ratio, for all tracks. Thanks to the
  // keyframe sorting, the loop can end as soon as it finds a key greater that
  // _ratio. It will mean that all the keys lower than _ratio have been
  // processed, meaning all cache entries are up to date.
  while (cursor < _keys.end() &&
         _keys[_cache[cursor->track * 2 + 1]].ratio <= _ratio) {
    // Flag this soa entry as outdated.
    _outdated[cursor->track / 32] |= (1 << ((cursor->track & 0x1f) / 4));
    // Updates cache.
    const int base = cursor->track * 2;
    _cache[base] = _cache[base + 1];
    _cache[base + 1] = static_cast<int>(cursor - _keys.begin());
    // Process next key.
    ++cursor;
  }
    Assert(cursor <= _keys.end());

  // Updates cursor output.
  *_cursor = static_cast<int>(cursor - _keys.begin());
}

template <typename _Key, typename _InterpKey, typename _Decompress>
void UpdateInterpKeyframes(int _num_soa_tracks,
                           const ozz::span<const _Key>& _keys,
                           const int* _interp, uint8_t* _outdated,
                           _InterpKey* _interp_keys,
                           const _Decompress& _decompress) {
  const int num_outdated_flags = (_num_soa_tracks + 7) / 8;
  for (int j = 0; j < num_outdated_flags; ++j) {
    uint8_t outdated = _outdated[j];
    _outdated[j] = 0;  // Reset outdated entries as all will be processed.
    for (int i = j * 8; outdated; ++i, outdated >>= 1) {
      if (!(outdated & 1)) {
        continue;
      }
      const int base = i * 4 * 2;  // * soa size * 2 keys

      // Decompress left side keyframes and store them in soa structures.
      const _Key& k00 = _keys[_interp[base + 0]];
      const _Key& k10 = _keys[_interp[base + 2]];
      const _Key& k20 = _keys[_interp[base + 4]];
      const _Key& k30 = _keys[_interp[base + 6]];
      _interp_keys[i].ratio[0] =
          math::simd_float4::Load(k00.ratio, k10.ratio, k20.ratio, k30.ratio);
      _decompress(k00, k10, k20, k30, &_interp_keys[i].value[0]);

      // Decompress right side keyframes and store them in soa structures.
      const _Key& k01 = _keys[_interp[base + 1]];
      const _Key& k11 = _keys[_interp[base + 3]];
      const _Key& k21 = _keys[_interp[base + 5]];
      const _Key& k31 = _keys[_interp[base + 7]];
      _interp_keys[i].ratio[1] =
          math::simd_float4::Load(k01.ratio, k11.ratio, k21.ratio, k31.ratio);
      _decompress(k01, k11, k21, k31, &_interp_keys[i].value[1]);
    }
  }
}

void Interpolates(float _anim_ratio, int _num_soa_tracks,
                  const internal::InterpSoaFloat3* _translations,
                  const internal::InterpSoaQuaternion* _rotations,
                  const internal::InterpSoaFloat3* _scales,
                  math::SoaTransform* _output) {
  const math::SimdFloat4 anim_ratio = math::simd_float4::Load1(_anim_ratio);
  for (int i = 0; i < _num_soa_tracks; ++i) {
    // Prepares interpolation coefficients.
    const math::SimdFloat4 interp_t_ratio =
        (anim_ratio - _translations[i].ratio[0]) *
        math::RcpEst(_translations[i].ratio[1] - _translations[i].ratio[0]);
    const math::SimdFloat4 interp_r_ratio =
        (anim_ratio - _rotations[i].ratio[0]) *
        math::RcpEst(_rotations[i].ratio[1] - _rotations[i].ratio[0]);
    const math::SimdFloat4 interp_s_ratio =
        (anim_ratio - _scales[i].ratio[0]) *
        math::RcpEst(_scales[i].ratio[1] - _scales[i].ratio[0]);

    // Processes interpolations.
    // The lerp of the rotation uses the shortest path, because opposed
    // quaternions were negated during animation build stage (AnimationBuilder).
    _output[i].translation = Lerp(_translations[i].value[0],
                                  _translations[i].value[1], interp_t_ratio);
    _output[i].rotation = NLerpEst(_rotations[i].value[0],
                                   _rotations[i].value[1], interp_r_ratio);
    _output[i].scale =
        Lerp(_scales[i].value[0], _scales[i].value[1], interp_s_ratio);
  }
}
}  // namespace

SamplingJob::SamplingJob() : ratio(0.f), animation(nullptr), cache(nullptr) {}

void SamplingJob::Run() const
{
  Assert(Validate());
  const int num_soa_tracks = animation->num_soa_tracks();
  Expect(num_soa_tracks > 0);

  // Clamps ratio in range [0,duration].
  const float anim_ratio = math::Clamp(0.f, ratio, 1.f);

  // Step the cache to this potentially new animation and ratio.
    Assert(cache->max_soa_tracks() >= num_soa_tracks);
  cache->Step(*animation, anim_ratio);

  // Fetch key frames from the animation to the cache a r = anim_ratio.
  // Then updates outdated soa hot values.
  UpdateCacheCursor(anim_ratio, num_soa_tracks, animation->translations(),
                    &cache->translation_cursor_, cache->translation_keys_,
                    cache->outdated_translations_);
  UpdateInterpKeyframes(num_soa_tracks, animation->translations(),
                        cache->translation_keys_, cache->outdated_translations_,
                        cache->soa_translations_, &Float3Key::DecompressSoA);

  UpdateCacheCursor(anim_ratio, num_soa_tracks, animation->rotations(),
                    &cache->rotation_cursor_, cache->rotation_keys_,
                    cache->outdated_rotations_);
  UpdateInterpKeyframes(num_soa_tracks, animation->rotations(),
                        cache->rotation_keys_, cache->outdated_rotations_,
                        cache->soa_rotations_, &QuaternionKey::DecompressSoA);

  UpdateCacheCursor(anim_ratio, num_soa_tracks, animation->scales(),
                    &cache->scale_cursor_, cache->scale_keys_,
                    cache->outdated_scales_);
  UpdateInterpKeyframes(num_soa_tracks, animation->scales(), cache->scale_keys_,
                        cache->outdated_scales_, cache->soa_scales_,
                        &Float3Key::DecompressSoA);

  // Interpolates soa hot data.
  Interpolates(anim_ratio, num_soa_tracks, cache->soa_translations_,
               cache->soa_rotations_, cache->soa_scales_, output.begin());
}

SamplingCache::SamplingCache()
    : max_soa_tracks_(0),
      soa_translations_(
          nullptr) {  // soa_translations_ is the allocation pointer.
  Invalidate();
}

SamplingCache::SamplingCache(const SamplingCache& other)
  : animation_(other.animation_)
  , ratio_(other.ratio_)
  , translation_keys_(other.translation_keys_)
  , rotation_keys_(other.rotation_keys_)
  , scale_keys_(other.scale_keys_)
  , translation_cursor_(other.translation_cursor_)
  , rotation_cursor_(other.rotation_cursor_)
  , scale_cursor_(other.scale_cursor_)
{
  Resize(other.max_tracks());

  using internal::InterpSoaFloat3;
  using internal::InterpSoaQuaternion;

  // Computes allocation size.
  const size_t max_tracks = max_soa_tracks_ * 4;
  const size_t num_outdated = (max_soa_tracks_ + 7) / 8;
  const size_t size =
    sizeof(InterpSoaFloat3) * max_soa_tracks_ +
    sizeof(InterpSoaQuaternion) * max_soa_tracks_ +
    sizeof(InterpSoaFloat3) * max_soa_tracks_ +
    sizeof(int) * max_tracks * 2 * 3 +  // 2 keys * (trans + rot + scale).
    sizeof(uint8_t) * 3 * num_outdated;

  using namespace ngine;
  Memory::CopyNonOverlappingElements(reinterpret_cast<char*>(soa_translations_), reinterpret_cast<char*>(other.soa_translations_), size);
}

SamplingCache::SamplingCache(int _max_tracks)
    : max_soa_tracks_(0),
      soa_translations_(
          nullptr) {  // soa_translations_ is the allocation pointer.
  Resize(_max_tracks);
}

SamplingCache::~SamplingCache() {
  using internal::InterpSoaFloat3;
  // Deallocates everything at once.
  memory::default_allocator()->Deallocate(soa_translations_, alignof(InterpSoaFloat3));
}

void SamplingCache::Resize(int _max_tracks) {
  using internal::InterpSoaFloat3;
  using internal::InterpSoaQuaternion;

  // Reset existing data.
  Invalidate();
  memory::default_allocator()->Deallocate(soa_translations_, alignof(InterpSoaFloat3));

  // Updates maximum supported soa tracks.
  max_soa_tracks_ = (_max_tracks + 3) / 4;

  // Allocate all cache data at once in a single allocation.
  // Alignment is guaranteed because memory is dispatch from the highest
  // alignment requirement (Soa data: SimdFloat4) to the lowest (outdated
  // flag: unsigned char).

  // Computes allocation size.
  const size_t max_tracks = max_soa_tracks_ * 4;
  const size_t num_outdated = (max_soa_tracks_ + 7) / 8;
  const size_t size =
      sizeof(InterpSoaFloat3) * max_soa_tracks_ +
      sizeof(InterpSoaQuaternion) * max_soa_tracks_ +
      sizeof(InterpSoaFloat3) * max_soa_tracks_ +
      sizeof(int) * max_tracks * 2 * 3 +  // 2 keys * (trans + rot + scale).
      sizeof(uint8_t) * 3 * num_outdated;

  // Allocates all at once.
  memory::Allocator* allocator = memory::default_allocator();
  char* alloc_begin = reinterpret_cast<char*>(
      allocator->Allocate(size, alignof(InterpSoaFloat3)));
  char* alloc_cursor = alloc_begin;

  // Distributes buffer memory while ensuring proper alignment (serves larger
  // alignment values first).
  static_assert(alignof(InterpSoaFloat3) >= alignof(InterpSoaQuaternion) &&
                    alignof(InterpSoaQuaternion) >= alignof(InterpSoaFloat3) &&
                    alignof(InterpSoaFloat3) >= alignof(int) &&
                    alignof(int) >= alignof(uint8_t),
                "Must serve larger alignment values first)");

  soa_translations_ = reinterpret_cast<InterpSoaFloat3*>(alloc_cursor);
    Assert(IsAligned(soa_translations_, alignof(InterpSoaFloat3)));
  alloc_cursor += sizeof(InterpSoaFloat3) * max_soa_tracks_;
  soa_rotations_ = reinterpret_cast<InterpSoaQuaternion*>(alloc_cursor);
    Assert(IsAligned(soa_rotations_, alignof(InterpSoaQuaternion)));
  alloc_cursor += sizeof(InterpSoaQuaternion) * max_soa_tracks_;
  soa_scales_ = reinterpret_cast<InterpSoaFloat3*>(alloc_cursor);
    Assert(IsAligned(soa_scales_, alignof(InterpSoaFloat3)));
  alloc_cursor += sizeof(InterpSoaFloat3) * max_soa_tracks_;

  translation_keys_ = reinterpret_cast<int*>(alloc_cursor);
    Assert(IsAligned(translation_keys_, alignof(int)));
  alloc_cursor += sizeof(int) * max_tracks * 2;
  rotation_keys_ = reinterpret_cast<int*>(alloc_cursor);
  alloc_cursor += sizeof(int) * max_tracks * 2;
  scale_keys_ = reinterpret_cast<int*>(alloc_cursor);
  alloc_cursor += sizeof(int) * max_tracks * 2;

  outdated_translations_ = reinterpret_cast<uint8_t*>(alloc_cursor);
    Assert(IsAligned(outdated_translations_, alignof(uint8_t)));
  alloc_cursor += sizeof(uint8_t) * num_outdated;
  outdated_rotations_ = reinterpret_cast<uint8_t*>(alloc_cursor);
  alloc_cursor += sizeof(uint8_t) * num_outdated;
  outdated_scales_ = reinterpret_cast<uint8_t*>(alloc_cursor);
  alloc_cursor += sizeof(uint8_t) * num_outdated;

    Assert(alloc_cursor == alloc_begin + size);
}

void SamplingCache::Step(const Animation& _animation, float _ratio) {
  // The cache is invalidated if animation has changed or if it is being rewind.
  if (animation_ != &_animation || _ratio < ratio_) {
    animation_ = &_animation;
    translation_cursor_ = 0;
    rotation_cursor_ = 0;
    scale_cursor_ = 0;
  }
  ratio_ = _ratio;
}

void SamplingCache::Invalidate() {
  animation_ = nullptr;
  ratio_ = 0.f;
  translation_cursor_ = 0;
  rotation_cursor_ = 0;
  scale_cursor_ = 0;
}
}  // namespace animation
}  // namespace ozz
