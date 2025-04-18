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

// Copyright (c) ngine technologies
// This file is based off ozz-animation, see copyright notice above.
// Any changes on top of ozz is considered property of ngine technologies.

#include "SkinningJob.h"

#include <Common/Assert/Assert.h>
#include <Common/Platform/Unused.h>
#include <Common/Math/CompressedDirectionAndSign.h>

#include "3rdparty/ozz/base/maths/simd_math.h"

namespace ngine::Animation
{
	SkinningJob::SkinningJob()
		: vertex_count(0)
		, influences_count(0)
		, joint_indices_stride(0)
		, joint_weights_stride(0)
		, in_positions_stride(0)
		, in_normals_stride(0)
		, in_tangents_stride(0)
		, out_positions_stride(0)
		, out_normals_stride(0)
		, out_tangents_stride(0)
	{
	}

	bool SkinningJob::Validate() const
	{
		// Start validation of all parameters.
		bool valid = true;

		// Checks influences bounds.
		valid &= influences_count > 0;

		// Checks joints matrices, required.
		valid &= !joint_matrices.empty();

		// Prepares local variables used to compute buffer size.
		const int vertex_count_minus_1 = vertex_count > 0 ? vertex_count - 1 : 0;
		const int vertex_count_at_least_1 = vertex_count > 0;

		// Checks indices, required.
		valid &= joint_indices.size_bytes() >=
		         joint_indices_stride * vertex_count_minus_1 + sizeof(uint16_t) * influences_count * vertex_count_at_least_1;

		// Checks weights, required if influences_count > 1.
		if (influences_count != 1)
		{
			valid &= joint_weights.size_bytes() >=
			         joint_weights_stride * vertex_count_minus_1 + sizeof(float) * (influences_count - 1) * vertex_count_at_least_1;
		}

		// Checks positions, mandatory.
		valid &= in_positions.size_bytes() >= in_positions_stride * vertex_count_minus_1 + sizeof(float) * 3 * vertex_count_at_least_1;
		valid &= !out_positions.empty();
		valid &= out_positions.size_bytes() >= out_positions_stride * vertex_count_minus_1 + sizeof(float) * 3 * vertex_count_at_least_1;

		// Checks normals, optional.
		if (!in_normals.empty())
		{
			valid &= in_normals.size_bytes() >=
			         in_normals_stride * vertex_count_minus_1 + sizeof(Math::CompressedDirectionAndSign) * vertex_count_at_least_1;
			valid &= !out_normals.empty();
			valid &= out_normals.size_bytes() >=
			         out_normals_stride * vertex_count_minus_1 + sizeof(Math::CompressedDirectionAndSign) * vertex_count_at_least_1;

			// Checks tangents, optional but requires normals.
			if (!in_tangents.empty())
			{
				valid &= in_tangents.size_bytes() >=
				         in_tangents_stride * vertex_count_minus_1 + sizeof(Math::CompressedDirectionAndSign) * vertex_count_at_least_1;
				valid &= !out_tangents.empty();
				valid &= out_tangents.size_bytes() >=
				         out_tangents_stride * vertex_count_minus_1 + sizeof(Math::CompressedDirectionAndSign) * vertex_count_at_least_1;
			}
		}
		else
		{
			// Tangents are not supported if normals are not there.
			valid &= in_tangents.empty();
		}

		return valid;
	}

// For performance optimization reasons, every skinning variants (positions,
// positions + normals, 1 to n influences...) are implemented as separate
// specialized functions.
// To cope with the error prone aspect of implementing every function, we
// define a skeleton code (SKINNING_FN) for the skinning loop, which internally
// calls MACRO that are shared or specialized according to skinning variants.

// Defines the skeleton code for the per vertex skinning loop.
#define SKINNING_FN(_type, _it, _inf) \
	void SKINNING_FN_NAME(_type, _it, _inf)(const SkinningJob& _job) \
	{ \
		ASSERT_##_type() ASSERT_##_it() INIT_##_type() INIT_W##_inf() const int loops = _job.vertex_count - 1; \
		for (int i = 0; i < loops; ++i) \
		{ \
			PREPARE_##_inf##_INNER(_it) TRANSFORM_##_type##_INNER() NEXT_##_type() NEXT_W##_inf() \
		} \
		PREPARE_##_inf##_OUTER(_it) TRANSFORM_##_type##_OUTER() \
	}

// Defines skinning function name.
#define SKINNING_FN_NAME(_type, _it, _inf) Skinning##_type##_it##_inf

// Implements pre-conditions assertions.
#define ASSERT_P() Assert(_job.vertex_count && !_job.in_positions.empty() && _job.in_normals.empty());

#define ASSERT_PN() Assert(_job.vertex_count && !_job.in_positions.empty() && !_job.in_normals.empty() && _job.in_tangents.empty());

#define ASSERT_PNT() Assert(_job.vertex_count && !_job.in_positions.empty() && !_job.in_normals.empty() && !_job.in_tangents.empty());

#define ASSERT_NOIT()

#define ASSERT_IT() Assert(!_job.joint_inverse_transpose_matrices.empty());

// Implements loop initializations for positions, ...
#define INIT_P() \
	const uint16_t* __restrict joint_indices = _job.joint_indices.begin(); \
	const float* __restrict in_positions = _job.in_positions.begin(); \
	float* out_positions = _job.out_positions.begin();

#define INIT_PN() \
	INIT_P(); \
	const Math::CompressedDirectionAndSign* __restrict in_normals = _job.in_normals.begin(); \
	Math::CompressedDirectionAndSign* out_normals = _job.out_normals.begin();

#define INIT_PNT() \
	INIT_PN(); \
	const Math::CompressedDirectionAndSign* __restrict in_tangents = _job.in_tangents.begin(); \
	Math::CompressedDirectionAndSign* out_tangents = _job.out_tangents.begin();

// Implements loop initializations for weights.
// Note that if the number of influences per vertex is 1, then there's no weight
// as it's implicitly 1.
#define INIT_W1()

#define INIT_W2() \
	const ozz::math::SimdFloat4 one = ozz::math::simd_float4::one(); \
	const float* __restrict joint_weights = _job.joint_weights.begin();

#define INIT_W3() INIT_W2()

#define INIT_W4() INIT_W2()

#define INIT_WN() INIT_W2()

// Implements pointer striding.
#define NEXT(_type, _current, _stride) reinterpret_cast<_type>(reinterpret_cast<uintptr_t>(_current) + _stride)

#define NEXT_W1()

#define NEXT_W2() joint_weights = NEXT(const float*, joint_weights, _job.joint_weights_stride);

#define NEXT_W3() NEXT_W2()

#define NEXT_W4() NEXT_W2()

#define NEXT_WN() NEXT_W2()

#define NEXT_P() \
	joint_indices = NEXT(const uint16_t*, joint_indices, _job.joint_indices_stride); \
	in_positions = NEXT(const float*, in_positions, _job.in_positions_stride); \
	out_positions = NEXT(float*, out_positions, _job.out_positions_stride);

#define NEXT_PN() \
	NEXT_P(); \
	in_normals = NEXT(const Math::CompressedDirectionAndSign*, in_normals, _job.in_normals_stride); \
	out_normals = NEXT(Math::CompressedDirectionAndSign*, out_normals, _job.out_normals_stride);

#define NEXT_PNT() \
	NEXT_PN(); \
	in_tangents = NEXT(const Math::CompressedDirectionAndSign*, in_tangents, _job.in_tangents_stride); \
	out_tangents = NEXT(Math::CompressedDirectionAndSign*, out_tangents, _job.out_tangents_stride);

// Implements weighted matrix preparation.
// _INNER functions are intended to be used inside the vertex loop. They take
// advantage of the fact that the buffers they are reading from contain enough
// remaining data to use more optimized SIMD load functions. At the opposite,
// _OUTER functions restrict access to data that are sure to be readable from
// the buffer.
#define PREPARE_1_INNER(_it) \
	const uint16_t i0 = joint_indices[0]; \
	const ozz::math::Float4x4& transform = _job.joint_matrices[i0]; \
	PREPARE_##_it##_1()

#define PREPARE_1_OUTER(_it) PREPARE_1_INNER(_it)

#define PREPARE_NOIT() \
	const ozz::math::Float4x4& it_transform = transform; \
	(void)it_transform;

#define PREPARE_NOIT_1() PREPARE_NOIT()

#define PREPARE_IT_1() const ozz::math::Float4x4& it_transform = _job.joint_inverse_transpose_matrices[i0];

#define PREPARE_2_INNER(_it) \
	const ozz::math::SimdFloat4 w0 = ozz::math::simd_float4::Load1PtrU(joint_weights + 0); \
	const uint16_t i0 = joint_indices[0]; \
	const uint16_t i1 = joint_indices[1]; \
	const ozz::math::Float4x4& m0 = _job.joint_matrices[i0]; \
	const ozz::math::Float4x4& m1 = _job.joint_matrices[i1]; \
	const ozz::math::SimdFloat4 w1 = one - w0; \
	const ozz::math::Float4x4 transform = ozz::math::ColumnMultiply(m0, w0) + ozz::math::ColumnMultiply(m1, w1); \
	PREPARE_##_it##_2()

#define PREPARE_NOIT_2() PREPARE_NOIT()

#define PREPARE_IT_2() \
	const ozz::math::Float4x4& mit0 = _job.joint_inverse_transpose_matrices[i0]; \
	const ozz::math::Float4x4& mit1 = _job.joint_inverse_transpose_matrices[i1]; \
	const ozz::math::Float4x4 it_transform = ozz::math::ColumnMultiply(mit0, w0) + ozz::math::ColumnMultiply(mit1, w1);

#define PREPARE_2_OUTER(_it) PREPARE_2_INNER(_it)

#define PREPARE_3_CONCAT(_it) \
	const uint16_t i0 = joint_indices[0]; \
	const uint16_t i1 = joint_indices[1]; \
	const uint16_t i2 = joint_indices[2]; \
	const ozz::math::Float4x4& m0 = _job.joint_matrices[i0]; \
	const ozz::math::Float4x4& m1 = _job.joint_matrices[i1]; \
	const ozz::math::Float4x4& m2 = _job.joint_matrices[i2]; \
	const ozz::math::SimdFloat4 w2 = one - (w0 + w1); \
	const ozz::math::Float4x4 transform = ozz::math::ColumnMultiply(m0, w0) + ozz::math::ColumnMultiply(m1, w1) + \
	                                      ozz::math::ColumnMultiply(m2, w2); \
	PREPARE_##_it##_3()

#define PREPARE_NOIT_3() PREPARE_NOIT()

#define PREPARE_IT_3() \
	const ozz::math::Float4x4& mit0 = _job.joint_inverse_transpose_matrices[i0]; \
	const ozz::math::Float4x4& mit1 = _job.joint_inverse_transpose_matrices[i1]; \
	const ozz::math::Float4x4& mit2 = _job.joint_inverse_transpose_matrices[i2]; \
	const ozz::math::Float4x4 it_transform = ozz::math::ColumnMultiply(mit0, w0) + ozz::math::ColumnMultiply(mit1, w1) + \
	                                         ozz::math::ColumnMultiply(mit2, w2);

#define PREPARE_3_INNER(_it) \
	const ozz::math::SimdFloat4 w = ozz::math::simd_float4::LoadPtrU(joint_weights); \
	const ozz::math::SimdFloat4 w0 = ozz::math::SplatX(w); \
	const ozz::math::SimdFloat4 w1 = ozz::math::SplatY(w); \
	PREPARE_3_CONCAT(_it)

#define PREPARE_3_OUTER(_it) \
	const ozz::math::SimdFloat4 w0 = ozz::math::simd_float4::Load1PtrU(joint_weights + 0); \
	const ozz::math::SimdFloat4 w1 = ozz::math::simd_float4::Load1PtrU(joint_weights + 1); \
	PREPARE_3_CONCAT(_it)

#define PREPARE_4_CONCAT(_it) \
	const uint16_t i0 = joint_indices[0]; \
	const uint16_t i1 = joint_indices[1]; \
	const uint16_t i2 = joint_indices[2]; \
	const uint16_t i3 = joint_indices[3]; \
	const ozz::math::Float4x4& m0 = _job.joint_matrices[i0]; \
	const ozz::math::Float4x4& m1 = _job.joint_matrices[i1]; \
	const ozz::math::Float4x4& m2 = _job.joint_matrices[i2]; \
	const ozz::math::Float4x4& m3 = _job.joint_matrices[i3]; \
	const ozz::math::SimdFloat4 w3 = one - (w0 + w1 + w2); \
	const ozz::math::Float4x4 transform = ozz::math::ColumnMultiply(m0, w0) + ozz::math::ColumnMultiply(m1, w1) + \
	                                      ozz::math::ColumnMultiply(m2, w2) + ozz::math::ColumnMultiply(m3, w3); \
	PREPARE_##_it##_4()

#define PREPARE_NOIT_4() PREPARE_NOIT()

#define PREPARE_IT_4() \
	const ozz::math::Float4x4& mit0 = _job.joint_inverse_transpose_matrices[i0]; \
	const ozz::math::Float4x4& mit1 = _job.joint_inverse_transpose_matrices[i1]; \
	const ozz::math::Float4x4& mit2 = _job.joint_inverse_transpose_matrices[i2]; \
	const ozz::math::Float4x4& mit3 = _job.joint_inverse_transpose_matrices[i3]; \
	const ozz::math::Float4x4 it_transform = ozz::math::ColumnMultiply(mit0, w0) + ozz::math::ColumnMultiply(mit1, w1) + \
	                                         ozz::math::ColumnMultiply(mit2, w2) + ozz::math::ColumnMultiply(mit3, w3);

#define PREPARE_4_INNER(_it) \
	const ozz::math::SimdFloat4 w = ozz::math::simd_float4::LoadPtrU(joint_weights); \
	const ozz::math::SimdFloat4 w0 = ozz::math::SplatX(w); \
	const ozz::math::SimdFloat4 w1 = ozz::math::SplatY(w); \
	const ozz::math::SimdFloat4 w2 = ozz::math::SplatZ(w); \
	PREPARE_4_CONCAT(_it)

#define PREPARE_4_OUTER(_it) \
	const ozz::math::SimdFloat4 w0 = ozz::math::simd_float4::Load1PtrU(joint_weights + 0); \
	const ozz::math::SimdFloat4 w1 = ozz::math::simd_float4::Load1PtrU(joint_weights + 1); \
	const ozz::math::SimdFloat4 w2 = ozz::math::simd_float4::Load1PtrU(joint_weights + 2); \
	PREPARE_4_CONCAT(_it)

#define PREPARE_NOIT_N() \
	ozz::math::SimdFloat4 wsum = ozz::math::simd_float4::Load1PtrU(joint_weights + 0); \
	ozz::math::Float4x4 transform = ozz::math::ColumnMultiply(_job.joint_matrices[joint_indices[0]], wsum); \
	const int last = _job.influences_count - 1; \
	for (int j = 1; j < last; ++j) \
	{ \
		const ozz::math::SimdFloat4 w = ozz::math::simd_float4::Load1PtrU(joint_weights + j); \
		wsum = wsum + w; \
		transform = transform + ozz::math::ColumnMultiply(_job.joint_matrices[joint_indices[j]], w); \
	} \
	transform = transform + ozz::math::ColumnMultiply(_job.joint_matrices[joint_indices[last]], one - wsum); \
	PREPARE_NOIT()

#define PREPARE_IT_N() \
	ozz::math::SimdFloat4 wsum = ozz::math::simd_float4::Load1PtrU(joint_weights + 0); \
	const uint16_t i0 = joint_indices[0]; \
	ozz::math::Float4x4 transform = ozz::math::ColumnMultiply(_job.joint_matrices[i0], wsum); \
	ozz::math::Float4x4 it_transform = ozz::math::ColumnMultiply(_job.joint_inverse_transpose_matrices[i0], wsum); \
	const int last = _job.influences_count - 1; \
	for (int j = 1; j < last; ++j) \
	{ \
		const uint16_t ij = joint_indices[j]; \
		const ozz::math::SimdFloat4 w = ozz::math::simd_float4::Load1PtrU(joint_weights + j); \
		wsum = wsum + w; \
		transform = transform + ozz::math::ColumnMultiply(_job.joint_matrices[ij], w); \
		it_transform = it_transform + ozz::math::ColumnMultiply(_job.joint_inverse_transpose_matrices[ij], w); \
	} \
	const ozz::math::SimdFloat4 wlast = one - wsum; \
	const int ilast = joint_indices[last]; \
	transform = transform + ozz::math::ColumnMultiply(_job.joint_matrices[ilast], wlast); \
	it_transform = it_transform + ozz::math::ColumnMultiply(_job.joint_inverse_transpose_matrices[ilast], wlast);

#define PREPARE_N_INNER(_it) PREPARE_##_it##_N()

#define PREPARE_N_OUTER(_it) PREPARE_##_it##_N()

// Implement point and vector transformation. _INNER and _OUTER have the same
// meaning as defined for the PREPARE functions.
#define TRANSFORM_P_INNER() \
	const ozz::math::SimdFloat4 in_p = ozz::math::simd_float4::LoadPtrU(in_positions); \
	const ozz::math::SimdFloat4 out_p = TransformPoint(transform, in_p); \
	ozz::math::Store3PtrU(out_p, out_positions);

#define TRANSFORM_PN_INNER() \
	TRANSFORM_P_INNER(); \
	const Math::Vector3f in_n = *in_normals; \
	const ozz::math::SimdFloat4 out_n = TransformVector(it_transform, in_n.GetVectorized()); \
	*out_normals = Math::CompressedDirectionAndSign{Math::Vector3f(out_n)};

#define TRANSFORM_PNT_INNER() \
	TRANSFORM_PN_INNER(); \
	const Math::Vector4f in_t = *in_tangents; \
	const ozz::math::SimdFloat4 out_t = TransformVector(it_transform, in_t.GetVectorized()); \
	*out_tangents = Math::CompressedDirectionAndSign{Math::Vector4f(out_t)};

#define TRANSFORM_P_OUTER() \
	const ozz::math::SimdFloat4 in_p = ozz::math::simd_float4::Load3PtrU(in_positions); \
	const ozz::math::SimdFloat4 out_p = TransformPoint(transform, in_p); \
	ozz::math::Store3PtrU(out_p, out_positions);

#define TRANSFORM_PN_OUTER() \
	TRANSFORM_P_OUTER(); \
	const Math::Vector3f in_n = *in_normals; \
	const ozz::math::SimdFloat4 out_n = TransformVector(it_transform, in_n.GetVectorized()); \
	*out_normals = Math::CompressedDirectionAndSign{Math::Vector3f(out_n)};

#define TRANSFORM_PNT_OUTER() \
	TRANSFORM_PN_OUTER(); \
	const Math::Vector4f in_t = *in_tangents; \
	const ozz::math::SimdFloat4 out_t = TransformVector(it_transform, in_t.GetVectorized()); \
	*out_tangents = Math::CompressedDirectionAndSign{Math::Vector4f(out_t)};

	// Instantiates all skinning function variants.
	SKINNING_FN(P, NOIT, 1)
	SKINNING_FN(PN, NOIT, 1)
	SKINNING_FN(PNT, NOIT, 1)
	SKINNING_FN(PN, IT, 1)
	SKINNING_FN(PNT, IT, 1)
	SKINNING_FN(P, NOIT, 2)
	SKINNING_FN(PN, NOIT, 2)
	SKINNING_FN(PNT, NOIT, 2)
	SKINNING_FN(PN, IT, 2)
	SKINNING_FN(PNT, IT, 2)
	SKINNING_FN(P, NOIT, 3)
	SKINNING_FN(PN, NOIT, 3)
	SKINNING_FN(PNT, NOIT, 3)
	SKINNING_FN(PN, IT, 3)
	SKINNING_FN(PNT, IT, 3)
	SKINNING_FN(P, NOIT, 4)
	SKINNING_FN(PN, NOIT, 4)
	SKINNING_FN(PNT, NOIT, 4)
	SKINNING_FN(PN, IT, 4)
	SKINNING_FN(PNT, IT, 4)
	SKINNING_FN(P, NOIT, N)
	SKINNING_FN(PN, NOIT, N)
	SKINNING_FN(PNT, NOIT, N)
	SKINNING_FN(PN, IT, N)
	SKINNING_FN(PNT, IT, N)

	// Defines a matrix of skinning function pointers. This matrix will then be
	// indexed according to skinning jobs parameters.
	typedef void (*SkiningFct)(const SkinningJob&);
	static const SkiningFct kSkinningFct[2][5][3] = {
		{
			{&SKINNING_FN_NAME(P, NOIT, 1), &SKINNING_FN_NAME(PN, NOIT, 1), &SKINNING_FN_NAME(PNT, NOIT, 1)},
			{&SKINNING_FN_NAME(P, NOIT, 2), &SKINNING_FN_NAME(PN, NOIT, 2), &SKINNING_FN_NAME(PNT, NOIT, 2)},
			{&SKINNING_FN_NAME(P, NOIT, 3), &SKINNING_FN_NAME(PN, NOIT, 3), &SKINNING_FN_NAME(PNT, NOIT, 3)},
			{&SKINNING_FN_NAME(P, NOIT, 4), &SKINNING_FN_NAME(PN, NOIT, 4), &SKINNING_FN_NAME(PNT, NOIT, 4)},
			{&SKINNING_FN_NAME(P, NOIT, N), &SKINNING_FN_NAME(PN, NOIT, N), &SKINNING_FN_NAME(PNT, NOIT, N)},
		},
		{
			{&SKINNING_FN_NAME(P, NOIT, 1), &SKINNING_FN_NAME(PN, IT, 1), &SKINNING_FN_NAME(PNT, IT, 1)},
			{&SKINNING_FN_NAME(P, NOIT, 2), &SKINNING_FN_NAME(PN, IT, 2), &SKINNING_FN_NAME(PNT, IT, 2)},
			{&SKINNING_FN_NAME(P, NOIT, 3), &SKINNING_FN_NAME(PN, IT, 3), &SKINNING_FN_NAME(PNT, IT, 3)},
			{&SKINNING_FN_NAME(P, NOIT, 4), &SKINNING_FN_NAME(PN, IT, 4), &SKINNING_FN_NAME(PNT, IT, 4)},
			{&SKINNING_FN_NAME(P, NOIT, N), &SKINNING_FN_NAME(PN, IT, N), &SKINNING_FN_NAME(PNT, IT, N)},
		}
	};

	// Implements job Run function.
	void SkinningJob::Run() const
	{
		// Exit with an error if job is invalid.
		Assert(Validate(), "Run should never be called for invalid skinning jobs!");
		Assert(vertex_count > 0);

		// Find skinning function index.
		const size_t it = !joint_inverse_transpose_matrices.empty();
		Assert(it < OZZ_ARRAY_SIZE(kSkinningFct));
		const size_t inf = static_cast<size_t>(influences_count) > OZZ_ARRAY_SIZE(kSkinningFct[0]) ? OZZ_ARRAY_SIZE(kSkinningFct[0]) - 1
		                                                                                           : influences_count - 1;
		Assert(inf < OZZ_ARRAY_SIZE(kSkinningFct[0]));
		const size_t fct = !in_normals.empty() + !in_tangents.empty();
		Assert(fct < OZZ_ARRAY_SIZE(kSkinningFct[0][0]));

		// Calls skinning function. Cannot fail because job is valid.
		kSkinningFct[it][inf][fct](*this);
	}
}
