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

#define _CRT_SECURE_NO_WARNINGS 1

#include "3rdparty/ozz/animation/offline/skeleton_builder.h"

#include <cstring>

#include "3rdparty/ozz/animation/offline/raw_skeleton.h"
#include "3rdparty/ozz/animation/runtime/skeleton.h"
#include "3rdparty/ozz/base/containers/vector.h"
#include "3rdparty/ozz/base/maths/soa_transform.h"
#include "3rdparty/ozz/base/memory/allocator.h"

#include <Common/Memory/Copy.h>

namespace ozz {
namespace animation {
namespace offline {

namespace {
// Stores each traversed joint in a vector.
struct JointLister {
  explicit JointLister(int _num_joints) { linear_joints.reserve(_num_joints); }
  void operator()(const RawSkeleton::Joint& _current,
                  const RawSkeleton::Joint* _parent) {
    // Looks for the "lister" parent.
    int16_t parent = Skeleton::kNoParent;
    if (_parent) {
      // Start searching from the last joint.
      int16_t j = static_cast<int16_t>(linear_joints.size()) - 1;
      for (; j >= 0; --j) {
        if (linear_joints[j].joint == _parent) {
          parent = j;
          break;
        }
      }
      assert(parent >= 0);
    }
    const Joint listed = {&_current, parent};
    linear_joints.push_back(listed);
  }
  struct Joint {
    const RawSkeleton::Joint* joint;
    int16_t parent;
  };
  // Array of joints in the traversed DAG order.
  ozz::vector<Joint> linear_joints;
};
}  // namespace

// Validates the RawSkeleton and fills a Skeleton.
// Uses RawSkeleton::IterateJointsDF to traverse in DAG depth-first order.
// Building skeleton hierarchy in depth first order make it easier to iterate a
// skeleton sub-hierarchy.
unique_ptr<ozz::animation::Skeleton> SkeletonBuilder::operator()(
    const RawSkeleton& _raw_skeleton) const {
  // Tests _raw_skeleton validity.
  if (!_raw_skeleton.Validate()) {
    return nullptr;
  }

  // Everything is fine, allocates and fills the skeleton.
  // Will not fail.
  unique_ptr<ozz::animation::Skeleton> skeleton = make_unique<Skeleton>();
  const int num_joints = _raw_skeleton.num_joints();

  // Iterates through all the joint of the raw skeleton and fills a sorted joint
  // list.
  // Iteration order defines runtime skeleton joint ordering.
  JointLister lister(num_joints);
  IterateJointsDF<JointLister&>(_raw_skeleton, lister);
  assert(static_cast<int>(lister.linear_joints.size()) == num_joints);

  // Computes name's buffer size.
  size_t chars_size = 0;
  for (int i = 0; i < num_joints; ++i) {
    const RawSkeleton::Joint& current = *lister.linear_joints[i].joint;
    chars_size += (current.name.size() + 1) * sizeof(char);
  }

  // Allocates all skeleton members.
  char* cursor = skeleton->Allocate(chars_size, num_joints);

  // Copy names. All names are allocated in a single buffer. Only the first name
  // is set, all other names array entries must be initialized.
  for (int i = 0; i < num_joints; ++i) {
    const RawSkeleton::Joint& current = *lister.linear_joints[i].joint;
    skeleton->joint_names_[i] = cursor;
    using namespace ngine;
    Memory::CopyWithoutOverlap(cursor, current.name.c_str(), current.name.size());
    cursor[current.name.size()] = '\0';
    cursor += (current.name.size() + 1) * sizeof(char);
  }

  // Transfers sorted joints hierarchy to the new skeleton.
  for (int i = 0; i < num_joints; ++i) {
    skeleton->joint_parents_[i] = lister.linear_joints[i].parent;
  }

  // Transfers t-poses.
  const math::SimdFloat4 w_axis = math::simd_float4::w_axis();
  const math::SimdFloat4 zero = math::simd_float4::zero();
  const math::SimdFloat4 one = math::simd_float4::one();

  for (int i = 0; i < skeleton->num_soa_joints(); ++i) {
    math::SimdFloat4 translations[4];
    math::SimdFloat4 scales[4];
    math::SimdFloat4 rotations[4];
    for (int j = 0; j < 4; ++j) {
      if (i * 4 + j < num_joints) {
        const RawSkeleton::Joint& src_joint =
            *lister.linear_joints[i * 4 + j].joint;
        translations[j] =
            math::simd_float4::Load3PtrU(&src_joint.transform.translation.x);
        rotations[j] = math::NormalizeSafe4(
            math::simd_float4::LoadPtrU(&src_joint.transform.rotation.x),
            w_axis);
        scales[j] = math::simd_float4::Load3PtrU(&src_joint.transform.scale.x);
      } else {
        translations[j] = zero;
        rotations[j] = w_axis;
        scales[j] = one;
      }
    }
    // Fills the SoaTransform structure.
    math::Transpose4x3(translations,
                       &skeleton->joint_bind_poses_[i].translation.x);
    math::Transpose4x4(rotations, &skeleton->joint_bind_poses_[i].rotation.x);
    math::Transpose4x3(scales, &skeleton->joint_bind_poses_[i].scale.x);
  }

  return skeleton;  // Success.
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
