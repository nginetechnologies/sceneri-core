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

#include "3rdparty/ozz/animation/runtime/local_to_model_job.h"

#include <Common/Assert/Assert.h>

#include "3rdparty/ozz/base/maths/math_ex.h"
#include "3rdparty/ozz/base/maths/simd_math.h"
#include "3rdparty/ozz/base/maths/soa_float4x4.h"
#include "3rdparty/ozz/base/maths/soa_transform.h"

#include "3rdparty/ozz/animation/runtime/skeleton.h"

namespace ozz {
namespace animation {

LocalToModelJob::LocalToModelJob()
    : skeleton(nullptr),
      root(nullptr),
      from(Skeleton::kNoParent),
      to(Skeleton::kMaxJoints),
      from_excluded(false) {}

bool LocalToModelJob::Validate() const {
  // Don't need any early out, as jobs are valid in most of the performance
  // critical cases.
  // Tests are written in multiple lines in order to avoid branches.
  bool valid = true;

  // Test for nullptr begin pointers.
  if (!skeleton) {
    return false;
  }

  const size_t num_joints = static_cast<size_t>(skeleton->num_joints());
  const size_t num_soa_joints = (num_joints + 3) / 4;

  // Test input and output ranges, implicitly tests for nullptr end pointers.
  valid &= input.size() >= num_soa_joints;
  valid &= output.size() >= num_joints;

  return valid;
}

void LocalToModelJob::Run() const
{
  Assert(Validate());
  const span<const int16_t>& parents = skeleton->joint_parents();

  // Initializes an identity matrix that will be used to compute roots model
  // matrices without requiring a branch.
  const math::Float4x4 identity = math::Float4x4::identity();
  const math::Float4x4* root_matrix = (root == nullptr) ? &identity : root;

  // Applies hierarchical transformation.
  // Loop ends after "to".
  const int end = math::Min(to + 1, skeleton->num_joints());
  // Begins iteration from "from", or the next joint if "from" is excluded.
  // Process next joint if end is not reach. parents[begin] >= from is true as
  // long as "begin" is a child of "from".
  for (int i = math::Max(from + from_excluded, 0),
           process = i < end && (!from_excluded || parents[i] >= from);
       process;) {
    // Builds soa matrices from soa transforms.
    const math::SoaTransform& transform = input[i / 4];
    const math::SoaFloat4x4 local_soa_matrices = math::SoaFloat4x4::FromAffine(
        transform.translation, transform.rotation, transform.scale);

    // Converts to aos matrices.
    math::Float4x4 local_aos_matrices[4];
    math::Transpose16x16(&local_soa_matrices.cols[0].x,
                         local_aos_matrices->cols);

    // parents[i] >= from is true as long as "i" is a child of "from".
    for (const int soa_end = (i + 4) & ~3; i < soa_end && process;
         ++i, process = i < end && parents[i] >= from) {
      const int parent = parents[i];
      const math::Float4x4* parent_matrix =
          parent == Skeleton::kNoParent ? root_matrix : &output[parent];
      output[i] = *parent_matrix * local_aos_matrices[i & 3];
    }
  }
}
}  // namespace animation
}  // namespace ozz
