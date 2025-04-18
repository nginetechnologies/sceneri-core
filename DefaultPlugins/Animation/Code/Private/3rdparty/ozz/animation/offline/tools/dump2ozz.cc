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

#include "3rdparty/ozz/animation/offline/tools/import2ozz.h"

// Mocks OzzImporter so it can be used to dump default and reference
// configurations.
class DumpConverter : public ozz::animation::offline::OzzImporter {
 public:
  DumpConverter() {}
  ~DumpConverter() {}

 private:
  virtual bool Load(const char*) override { return true; }

  virtual bool Import(ozz::animation::offline::RawSkeleton*, const NodeType&) override {
    return true;
  }

  virtual AnimationNames GetAnimationNames() override { return AnimationNames(); }

  virtual bool Import(const char*, const ozz::animation::Skeleton&, float,
                      ozz::animation::offline::RawAnimation*) override {
    return true;
  }

  virtual NodeProperties GetNodeProperties(const char*) override {
    return NodeProperties();
  }

  virtual bool Import(const char*, const char*, const char*, NodeProperty::Type,
                      float, ozz::animation::offline::RawFloatTrack*) override {
    return true;
  }

  virtual bool Import(const char*, const char*, const char*, NodeProperty::Type,
                      float, ozz::animation::offline::RawFloat2Track*) override {
    return true;
  }

  virtual bool Import(const char*, const char*, const char*, NodeProperty::Type,
                      float, ozz::animation::offline::RawFloat3Track*) override {
    return true;
  }
  virtual bool Import(const char*, const char*, const char*, NodeProperty::Type,
                      float, ozz::animation::offline::RawFloat4Track*) override {
    return true;
  }
};
