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

#ifndef OZZ_OZZ_BASE_MATHS_INTERNAL_SIMD_MATH_CONFIG_H_
#define OZZ_OZZ_BASE_MATHS_INTERNAL_SIMD_MATH_CONFIG_H_

#include "Animation/3rdparty/ozz/base/platform.h"

// Avoid SIMD instruction detection if reference (aka scalar) implementation is
// forced.
#if !defined(OZZ_BUILD_SIMD_REF)

// Try to match a SSE2+ version.
#if USE_AVX2
#include <immintrin.h>
#define OZZ_SIMD_AVX2
#define OZZ_SIMD_AVX // avx is available if avx2 is.
#endif

#if defined(__FMA__) || defined(OZZ_SIMD_FMA)
#include <immintrin.h>
#define OZZ_SIMD_FMA
#endif

#if USE_AVX
#include <immintrin.h>
#define OZZ_SIMD_AVX
#define OZZ_SIMD_SSE4_2 // SSE4.2 is available if avx is.
#endif

#if USE_SSE4_2
#include <nmmintrin.h>
#define OZZ_SIMD_SSE4_2
#define OZZ_SIMD_SSE4_1 // SSE4.1 is available if SSE4.2 is.
#endif

#if USE_SSE4_1
#include <smmintrin.h>
#define OZZ_SIMD_SSE4_1
#define OZZ_SIMD_SSSE3 // SSSE3 is available if SSE4.1 is.
#endif

#if USE_SSSE3
#include <tmmintrin.h>
#define OZZ_SIMD_SSSE3
#define OZZ_SIMD_SSE3 // SSE3 is available if SSSE3 is.
#endif

#if USE_SSE3
#include <pmmintrin.h>
#define OZZ_SIMD_SSE3
#define OZZ_SIMD_SSE2 // SSE2 is available if SSE3 is.
#endif

// x64/amd64 have SSE2 instructions
// _M_IX86_FP is 2 if /arch:SSE2, /arch:AVX or /arch:AVX2 was used.
#if USE_SSE2
#include <emmintrin.h>
#define OZZ_SIMD_SSE2
#define OZZ_SIMD_SSEx // OZZ_SIMD_SSEx is the generic flag for SSE support
#endif

#include <Common/Math/Vectorization/Packed.h>
#include <Common/Math/Vectorization/PackedFloat.h>
#include <Common/Math/Vectorization/PackedInt32.h>

// End of SIMD instruction detection
#endif // !OZZ_BUILD_SIMD_REF

// SEE* intrinsics available
#if defined(OZZ_SIMD_SSEx)

namespace ozz
{
	namespace math
	{

		// Vector of four floating point values.
		typedef __m128 SimdFloat4;

		// Argument type for Float4.
		typedef const __m128 _SimdFloat4;

		// Vector of four integer values.
		typedef __m128i SimdInt4;

		// Argument type for Int4.
		typedef const __m128i _SimdInt4;
	} // namespace math
} // namespace ozz

#else // No builtin simd available

// No simd instruction set detected, switch back to reference implementation.
// OZZ_SIMD_REF is the generic flag for SIMD reference implementation.
#define OZZ_SIMD_REF

// Declares reference simd float and integer vectors outside of ozz::math, in
// order to match non-reference implementation details.

// Vector of four floating point values.
struct alignas(16) SimdFloat4Def
{
	using PackedType = ngine::Math::Vectorization::Packed<float, 4>;

	SimdFloat4Def()
	{
	}
	SimdFloat4Def(const float _x, const float _y, const float _z)
		: x(_x)
		, y(_y)
		, z(_z)
	{
	}
	SimdFloat4Def(const float _x, const float _y, const float _z, const float _w)
		: x(_x)
		, y(_y)
		, z(_z)
		, w(_w)
	{
	}
	SimdFloat4Def(const PackedType value)
		: m_packed(value)
	{
	}

	[[nodiscard]] FORCE_INLINE operator PackedType() const
	{
		return m_packed;
	}

	union
	{
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
		PackedType m_packed;
	};
};

// Vector of four integer values.
struct alignas(16) SimdInt4Def
{
	using PackedType = ngine::Math::Vectorization::Packed<int, 4>;

	SimdInt4Def()
	{
	}
	SimdInt4Def(const int _x, const int _y, const int _z)
		: x(_x)
		, y(_y)
		, z(_z)
	{
	}
	SimdInt4Def(const int _x, const int _y, const int _z, const int _w)
		: x(_x)
		, y(_y)
		, z(_z)
		, w(_w)
	{
	}
	SimdInt4Def(const PackedType value)
		: m_packed(value)
	{
	}

	[[nodiscard]] FORCE_INLINE operator PackedType() const
	{
		return m_packed;
	}

	union
	{
		struct
		{
			int x;
			int y;
			int z;
			int w;
		};
		PackedType m_packed;
	};
};

namespace ozz
{
	namespace math
	{

		// Vector of four floating point values.
		typedef SimdFloat4Def SimdFloat4;

		// Argument type for SimdFloat4
		typedef const SimdFloat4& _SimdFloat4;

		// Vector of four integer values.
		typedef SimdInt4Def SimdInt4;

		// Argument type for SimdInt4.
		typedef const SimdInt4& _SimdInt4;

	} // namespace math
} // namespace ozz
#endif // OZZ_SIMD_x

// Native SIMD operator already exist on some compilers, so they have to be disable from ozz implementation
#if !defined(OZZ_SIMD_REF) && (defined(__GNUC__) || defined(__llvm__))
#define OZZ_DISABLE_SSE_NATIVE_OPERATORS
#endif
#endif // OZZ_OZZ_BASE_MATHS_INTERNAL_SIMD_MATH_CONFIG_H_
