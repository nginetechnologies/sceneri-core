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

#ifndef OZZ_ANIMATION_RUNTIME_ANIMATION_KEYFRAME_H_
#define OZZ_ANIMATION_RUNTIME_ANIMATION_KEYFRAME_H_

#include "Animation/3rdparty/ozz/base/platform.h"
#ifndef OZZ_INCLUDE_PRIVATE_HEADER
#error "This header is private, it cannot be included from public headers."
#endif  // OZZ_INCLUDE_PRIVATE_HEADER

#include "Animation/3rdparty/ozz/base/maths/simd_math.h"
#include "Animation/3rdparty/ozz/base/maths/soa_quaternion.h"
#include "Animation/3rdparty/ozz/base/maths/soa_float.h"
#include "Animation/3rdparty/ozz/base/maths/vec_float.h"
#include "Animation/3rdparty/ozz/base/maths/quaternion.h"

#include "Common/Math/CoreNumericTypes.h"
#include <Common/Math/Vector4.h>

#include <algorithm>

namespace ozz {
namespace animation {

// Define animation key frame types (translation, rotation, scale). Every type
// as the same base made of the key time ratio and it's track index. This is
// required as key frames are not sorted per track, but sorted by ratio to favor
// cache coherency. Key frame values are compressed, according on their type.
// Decompression is efficient because it's done on SoA data and cached during
// sampling.

// Defines the float3 key frame type, used for translations and scales.
// Translation values are stored as half precision floats with 16 bits per
// component.
struct Float3Key
{
  Float3Key(const float ratio_, const uint16_t track_, const math::Float3& value_)
	  : ratio(ratio_)
	  , track(track_)
	  , value{ math::FloatToHalf(value_.x), math::FloatToHalf(value_.y), math::FloatToHalf(value_.z) }
  {
  }
  Float3Key(const float ratio_, const uint16_t track_, ngine::Math::IdentityType)
	  : ratio(ratio_)
	  , track(track_)
	  , value{ 0, 0, 0 }
  {
  }

  static inline void DecompressSoA(const Float3Key& _k0, const Float3Key& _k1,
	  const Float3Key& _k2, const Float3Key& _k3,
	  math::SoaFloat3* _soa_float3)
  {
	  _soa_float3->x = math::HalfToFloat(math::simd_int4::Load(
		  _k0.value[0], _k1.value[0], _k2.value[0], _k3.value[0]));
	  _soa_float3->y = math::HalfToFloat(math::simd_int4::Load(
		  _k0.value[1], _k1.value[1], _k2.value[1], _k3.value[1]));
	  _soa_float3->z = math::HalfToFloat(math::simd_int4::Load(
		  _k0.value[2], _k1.value[2], _k2.value[2], _k3.value[2]));
  }

  [[nodiscard]] inline math::Float3 Decompress() const
  {
	  return
	  {
		  math::HalfToFloat(value[0]),
		  math::HalfToFloat(value[1]),
		  math::HalfToFloat(value[2]),
	  };
  }

  float ratio;
  uint16_t track;
  uint16_t value[3];
};

// Defines the rotation key frame type.
// Rotation value is a quaternion. Quaternion are normalized, which means each
// component is in range [0:1]. This property allows to quantize the 3
// components to 3 signed integer 16 bits values. The 4th component is restored
// at runtime, using the knowledge that |w| = sqrt(1 - (a^2 + b^2 + c^2)).
// The sign of this 4th component is stored using 1 bit taken from the track
// member.
//
// In more details, compression algorithm stores the 3 smallest components of
// the quaternion and restores the largest. The 3 smallest can be pre-multiplied
// by sqrt(2) to gain some precision indeed.
//
// Quantization could be reduced to 11-11-10 bits as often used for animation
// key frames, but in this case RotationKey structure would induce 16 bits of
// padding.
struct QuaternionKey
{
  QuaternionKey(const float ratio_, const uint16_t track_, const math::Quaternion& value_)
	  : ratio(ratio_)
	  , track(track_)
  {
	static auto lessAbs = [](const float left, const float right)
	{
		return std::abs(left) < std::abs(right);
	};

	// Finds the largest quaternion component.
	const float quat[4] = { value_.x, value_.y, value_.z, value_.w };
	const size_t largest_ = std::max_element(quat, quat + 4, lessAbs) - quat;
	assert(largest_ <= 3);
	largest = largest_ & 0x3;

	// Stores the sign of the largest component.
	sign = quat[largest] < 0.f;

	// Quantize the 3 smallest components on 16 bits signed integers.
	const float kFloat2Int = 32767.f * math::kSqrt2;
	const int kMapping[4][3] = { {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2} };
	const int* map = kMapping[largest];
	const int a = static_cast<int>(floor(quat[map[0]] * kFloat2Int + .5f));
	const int b = static_cast<int>(floor(quat[map[1]] * kFloat2Int + .5f));
	const int c = static_cast<int>(floor(quat[map[2]] * kFloat2Int + .5f));
	value[0] = math::Clamp(-32767, a, 32767) & 0xffff;
	value[1] = math::Clamp(-32767, b, 32767) & 0xffff;
	value[2] = math::Clamp(-32767, c, 32767) & 0xffff;
  }
  QuaternionKey(const float ratio_, const uint16_t track_, const ngine::Math::IdentityType)
	  : QuaternionKey(ratio_, track_, math::Quaternion(0, 0, 0, 1))
  {
  }

  inline static void DecompressSoA(const QuaternionKey& _k0, const QuaternionKey& _k1,
	  const QuaternionKey& _k2, const QuaternionKey& _k3,
	  math::SoaQuaternion* _quaternion)
  {
	  // Defines a mapping table that defines components assignation in the output
	// quaternion.
	  constexpr int kCpntMapping[4][4] = {
		  {0, 0, 1, 2}, {0, 0, 1, 2}, {0, 1, 0, 2}, {0, 1, 2, 0} };

	  // Selects proper mapping for each key.
	  const int* m0 = kCpntMapping[_k0.largest];
	  const int* m1 = kCpntMapping[_k1.largest];
	  const int* m2 = kCpntMapping[_k2.largest];
	  const int* m3 = kCpntMapping[_k3.largest];

	  // Prepares an array of input values, according to the mapping required to
	  // restore quaternion largest component.
	  alignas(16) int cmp_keys[4][4] = {
		  {_k0.value[m0[0]], _k1.value[m1[0]], _k2.value[m2[0]], _k3.value[m3[0]]},
		  {_k0.value[m0[1]], _k1.value[m1[1]], _k2.value[m2[1]], _k3.value[m3[1]]},
		  {_k0.value[m0[2]], _k1.value[m1[2]], _k2.value[m2[2]], _k3.value[m3[2]]},
		  {_k0.value[m0[3]], _k1.value[m1[3]], _k2.value[m2[3]], _k3.value[m3[3]]},
	  };

	  // Resets largest component to 0. Overwritting here avoids 16 branchings
	  // above.
	  cmp_keys[_k0.largest][0] = 0;
	  cmp_keys[_k1.largest][1] = 0;
	  cmp_keys[_k2.largest][2] = 0;
	  cmp_keys[_k3.largest][3] = 0;

	  // Rebuilds quaternion from quantized values.
	  const math::SimdFloat4 kInt2Float =
		  math::simd_float4::Load1(1.f / (32767.f * math::kSqrt2));
	  math::SimdFloat4 cpnt[4] = {
		  kInt2Float *
			  math::simd_float4::FromInt(math::simd_int4::LoadPtr(cmp_keys[0])),
		  kInt2Float *
			  math::simd_float4::FromInt(math::simd_int4::LoadPtr(cmp_keys[1])),
		  kInt2Float *
			  math::simd_float4::FromInt(math::simd_int4::LoadPtr(cmp_keys[2])),
		  kInt2Float *
			  math::simd_float4::FromInt(math::simd_int4::LoadPtr(cmp_keys[3])),
	  };

	  // Get back length of 4th component. Favors performance over accuracy by using
	  // x * RSqrtEst(x) instead of Sqrt(x).
	  // ww0 cannot be 0 because we 're recomputing the largest component.
	  const math::SimdFloat4 dot = cpnt[0] * cpnt[0] + cpnt[1] * cpnt[1] +
		  cpnt[2] * cpnt[2] + cpnt[3] * cpnt[3];
	  const math::SimdFloat4 ww0 = math::Max(math::simd_float4::Load1(1e-16f),
		  math::simd_float4::one() - dot);
	  const math::SimdFloat4 w0 = ww0 * math::RSqrtEst(ww0);
	  // Re-applies 4th component' s sign.
	  const math::SimdInt4 newSign = math::ShiftL(
		  math::simd_int4::Load(_k0.sign, _k1.sign, _k2.sign, _k3.sign), 31);
	  const math::SimdFloat4 restored = math::Or(w0, newSign);

	  // Re-injects the largest component inside the SoA structure.
	  cpnt[_k0.largest] = math::Or(
		  cpnt[_k0.largest], math::And(restored, math::simd_int4::mask_f000()));
	  cpnt[_k1.largest] = math::Or(
		  cpnt[_k1.largest], math::And(restored, math::simd_int4::mask_0f00()));
	  cpnt[_k2.largest] = math::Or(
		  cpnt[_k2.largest], math::And(restored, math::simd_int4::mask_00f0()));
	  cpnt[_k3.largest] = math::Or(
		  cpnt[_k3.largest], math::And(restored, math::simd_int4::mask_000f()));

	  // Stores result.
	  _quaternion->x = cpnt[0];
	  _quaternion->y = cpnt[1];
	  _quaternion->z = cpnt[2];
	  _quaternion->w = cpnt[3];
  }

  [[nodiscard]] inline math::Quaternion Decompress() const
  {
	  ozz::math::SoaQuaternion output;
	  DecompressSoA(
		  *this,
		  QuaternionKey(0.f, 0, ngine::Math::Identity),
		  QuaternionKey(0.f, 0, ngine::Math::Identity),
		  QuaternionKey(0.f, 0, ngine::Math::Identity),
		  &output
	  );
	  return
	  {
		  ozz::math::GetX(output.x),
		  ozz::math::GetX(output.y),
		  ozz::math::GetX(output.z),
		  ozz::math::GetX(output.w)
	  };
  }

  float ratio;
  uint16_t track : 13;   // The track this key frame belongs to.
  uint16_t largest : 2;  // The largest component of the quaternion.
  uint16_t sign : 1;     // The sign of the largest component. 1 for negative.
  int16_t value[3];      // The quantized value of the 3 smallest components.
};

}  // namespace animation
}  // namespace ozz
#endif  // OZZ_ANIMATION_RUNTIME_ANIMATION_KEYFRAME_H_
