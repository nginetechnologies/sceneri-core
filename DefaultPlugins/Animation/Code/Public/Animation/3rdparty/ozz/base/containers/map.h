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

#ifndef OZZ_OZZ_BASE_CONTAINERS_MAP_H_
#define OZZ_OZZ_BASE_CONTAINERS_MAP_H_

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)  // warning C4702: unreachable code
#endif                           // _MSC_VER

#include <cstring>
#include <map>

#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER

#include "Animation/3rdparty/ozz/base/containers/std_allocator.h"

namespace ozz {
// Redirects std::map to ozz::map in order to replace std default allocator by
// ozz::StdAllocator.
template <class _Key, class _Ty, class _Pred = std::less<_Key>,
          class _Allocator = ozz::StdAllocator<std::pair<const _Key, _Ty>>>
using map = std::map<_Key, _Ty, _Pred, _Allocator>;

// Implements a string comparator that can be used by std algorithm like maps.
struct str_less {
  bool operator()(const char* const& _left, const char* const& _right) const {
    return strcmp(_left, _right) < 0;
  }
};

// Specializes std::map to use c-string as a key.
template <class _Ty, class _Allocator =
                         ozz::StdAllocator<std::pair<const char* const, _Ty>>>
using cstring_map = std::map<const char*, _Ty, str_less, _Allocator>;

// Redirects std::multimap to ozz::MultiMap in order to replace std default
// allocator by ozz::StdAllocator.
template <class _Key, class _Ty, class _Pred = std::less<_Key>,
          class _Allocator = ozz::StdAllocator<std::pair<const _Key, _Ty>>>
using multimap = std::multimap<_Key, _Ty, _Pred, _Allocator>;
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_MAP_H_
