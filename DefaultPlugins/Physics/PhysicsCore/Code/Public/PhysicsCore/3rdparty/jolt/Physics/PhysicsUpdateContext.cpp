// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/PhysicsUpdateContext.h>

JPH_NAMESPACE_BEGIN

PhysicsUpdateContext::PhysicsUpdateContext(TempAllocator &inTempAllocator) :
	mTempAllocator(&inTempAllocator),
	mSteps(inTempAllocator)
{
}

PhysicsUpdateContext::~PhysicsUpdateContext()
{
	JPH_ASSERT(mBodyPairs == nullptr);
	JPH_ASSERT(mActiveConstraints == nullptr);
}

JPH_NAMESPACE_END
