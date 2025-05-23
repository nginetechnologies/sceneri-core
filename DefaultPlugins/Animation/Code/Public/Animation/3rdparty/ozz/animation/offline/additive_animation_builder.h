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

#ifndef OZZ_OZZ_ANIMATION_OFFLINE_ADDITIVE_ANIMATION_BUILDER_H_
#define OZZ_OZZ_ANIMATION_OFFLINE_ADDITIVE_ANIMATION_BUILDER_H_

#include "Animation/3rdparty/ozz/base/platform.h"
#include "Animation/3rdparty/ozz/base/span.h"

namespace ozz {

namespace math {
struct Transform;
}

namespace animation {
namespace offline {

// Forward declare offline animation type.
struct RawAnimation;

// Defines the class responsible for building a delta animation from an offline
// raw animation. This is used to create animations compatible with additive
// blending.
class AdditiveAnimationBuilder {
 public:
  // Initializes the builder.
  AdditiveAnimationBuilder();

  // Builds delta animation from _input..
  // Returns true on success and fills _output_animation with the delta
  // version of _input animation.
  // *_output must be a valid RawAnimation instance. Uses first frame as
  // reference pose Returns false on failure and resets _output to an empty
  // animation. See RawAnimation::Validate() for more details about failure
  // reasons.
  bool operator()(const RawAnimation& _input, RawAnimation* _output) const;

  // Builds delta animation from _input..
  // Returns true on success and fills _output_animation with the delta
  // *_output must be a valid RawAnimation instance.
  // version of _input animation.
  // *_reference_pose used as the base pose to calculate deltas from
  // Returns false on failure and resets _output to an empty animation.
  bool operator()(const RawAnimation& _input,
                  const span<const math::Transform>& _reference_pose,
                  RawAnimation* _output) const;
};
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_OFFLINE_ADDITIVE_ANIMATION_BUILDER_H_
