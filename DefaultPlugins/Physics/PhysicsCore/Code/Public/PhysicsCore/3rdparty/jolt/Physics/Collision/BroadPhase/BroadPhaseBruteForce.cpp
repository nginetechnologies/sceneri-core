// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>
#include <Physics/Collision/BroadPhase/BroadPhaseBruteForce.h>
#include <Physics/Collision/RayCast.h>
#include <Physics/Collision/AABoxCast.h>
#include <Physics/Collision/CastResult.h>
#include <Physics/Body/BodyManager.h>
#include <Physics/Body/BodyPair.h>
#include <Geometry/RayAABox.h>
#include <Geometry/OrientedBox.h>
#include <Core/QuickSort.h>

JPH_NAMESPACE_BEGIN
	
void BroadPhaseBruteForce::AddBodiesFinalize(BodyID *ioBodies, int inNumber, AddState inAddState)
{ 
	lock_guard lock(mMutex);

	BodyView bodies = mBodyManager->GetBodies();

	// Allocate space
	uint32 idx = (uint32)mBodyIDs.size();
	mBodyIDs.resize(idx + inNumber);

	// Add bodies
	for (const BodyID *b = ioBodies, *b_end = ioBodies + inNumber; b < b_end; ++b)
	{
		Body &body = *bodies[*b];

		// Validate that body ID is consistent with array index
		JPH_ASSERT(body.GetID() == *b);
		JPH_ASSERT(!body.IsInBroadPhase());

		// Add it to the list
		mBodyIDs[idx] = body.GetID();
		++idx;

		// Indicate body is in the broadphase
		body.SetInBroadPhaseInternal(true);
	}

	// Resort
	QuickSort(mBodyIDs.begin(), mBodyIDs.end());
}
	
void BroadPhaseBruteForce::RemoveBodies(BodyID *ioBodies, int inNumber) 
{ 
	lock_guard lock(mMutex);

	BodyView bodies = mBodyManager->GetBodies();

	JPH_ASSERT((int)mBodyIDs.size() >= inNumber);

	// Remove bodies
	for (const BodyID *b = ioBodies, *b_end = ioBodies + inNumber; b < b_end; ++b)
	{
		Body &body = *bodies[*b];

		// Validate that body ID is consistent with array index
		JPH_ASSERT(body.GetID() == *b);
		JPH_ASSERT(body.IsInBroadPhase());

		// Find body id
		Array<BodyID>::iterator it = lower_bound(mBodyIDs.begin(), mBodyIDs.end(), body.GetID());
		JPH_ASSERT(it != mBodyIDs.end());

		// Remove element
		mBodyIDs.erase(it);

		// Indicate body is no longer in the broadphase
		body.SetInBroadPhaseInternal(false);
	}
}

void BroadPhaseBruteForce::NotifyBodiesAABBChanged(BodyID *ioBodies, int inNumber, bool inTakeLock) 
{
	// Do nothing, we directly reference the body
}

void BroadPhaseBruteForce::NotifyBodiesLayerChanged(BodyID * ioBodies, int inNumber)
{
	// Do nothing, we directly reference the body
}

void BroadPhaseBruteForce::CastRay(const RayCast &inRay, RayCastBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{ 
	shared_lock lock(mMutex);

	// Load ray
	Vec3 origin(inRay.mOrigin);
	RayInvDirection inv_direction(inRay.mDirection);

	// For all bodies
	float early_out_fraction = ioCollector.GetEarlyOutFraction();
	for (BodyID b : mBodyIDs)
	{
		const Body &body = mBodyManager->GetBody(b);

		// Test layer
		if (inObjectLayerFilter.ShouldCollide(body.GetObjectLayer()))
		{
			// Test intersection with ray
			const AABox &bounds = body.GetWorldSpaceBounds();
			float fraction = RayAABox(origin, inv_direction, bounds.mMin, bounds.mMax);
			if (fraction < early_out_fraction)
			{
				// Store hit
				BroadPhaseCastResult result { b, fraction };
				ioCollector.AddHit(result);
				if (ioCollector.ShouldEarlyOut())
					break;
				early_out_fraction = ioCollector.GetEarlyOutFraction();
			}
		}
	}
}

void BroadPhaseBruteForce::CollideAABox(const AABox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const 
{
	shared_lock lock(mMutex);

	// For all bodies
	for (BodyID b : mBodyIDs)
	{
		const Body &body = mBodyManager->GetBody(b);

		// Test layer
		if (inObjectLayerFilter.ShouldCollide(body.GetObjectLayer()))
		{
			// Test intersection with box
			const AABox &bounds = body.GetWorldSpaceBounds();
			if (bounds.Overlaps(inBox))
			{
				// Store hit
				ioCollector.AddHit(b);
				if (ioCollector.ShouldEarlyOut())
					break;
			}
		}
	}
}

void BroadPhaseBruteForce::CollideSphere(Vec3Arg inCenter, float inRadius, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	shared_lock lock(mMutex);

	float radius_sq = Square(inRadius);

	// For all bodies
	for (BodyID b : mBodyIDs)
	{
		const Body &body = mBodyManager->GetBody(b);

		// Test layer
		if (inObjectLayerFilter.ShouldCollide(body.GetObjectLayer()))
		{
			// Test intersection with box
			const AABox &bounds = body.GetWorldSpaceBounds();
			if (bounds.GetSqDistanceTo(inCenter) <= radius_sq)
			{
				// Store hit
				ioCollector.AddHit(b);
				if (ioCollector.ShouldEarlyOut())
					break;
			}
		}
	}
}

void BroadPhaseBruteForce::CollidePoint(Vec3Arg inPoint, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	shared_lock lock(mMutex);

	// For all bodies
	for (BodyID b : mBodyIDs)
	{
		const Body &body = mBodyManager->GetBody(b);

		// Test layer
		if (inObjectLayerFilter.ShouldCollide(body.GetObjectLayer()))
		{
			// Test intersection with box
			const AABox &bounds = body.GetWorldSpaceBounds();
			if (bounds.Contains(inPoint))
			{
				// Store hit
				ioCollector.AddHit(b);
				if (ioCollector.ShouldEarlyOut())
					break;
			}
		}
	}
}

void BroadPhaseBruteForce::CollideOrientedBox(const OrientedBox &inBox, CollideShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	shared_lock lock(mMutex);

	// For all bodies
	for (BodyID b : mBodyIDs)
	{
		const Body &body = mBodyManager->GetBody(b);

		// Test layer
		if (inObjectLayerFilter.ShouldCollide(body.GetObjectLayer()))
		{
			// Test intersection with box
			const AABox &bounds = body.GetWorldSpaceBounds();
			if (inBox.Overlaps(bounds))
			{
				// Store hit
				ioCollector.AddHit(b);
				if (ioCollector.ShouldEarlyOut())
					break;
			}
		}
	}
}

void BroadPhaseBruteForce::CastAABoxNoLock(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const 
{
	shared_lock lock(mMutex);

	// Load box
	Vec3 origin(inBox.mBox.GetCenter());
	Vec3 extent(inBox.mBox.GetExtent());
	RayInvDirection inv_direction(inBox.mDirection);

	// For all bodies
	float early_out_fraction = ioCollector.GetEarlyOutFraction();
	for (BodyID b : mBodyIDs)
	{
		const Body &body = mBodyManager->GetBody(b);

		// Test layer
		if (inObjectLayerFilter.ShouldCollide(body.GetObjectLayer()))
		{
			// Test intersection with ray
			const AABox &bounds = body.GetWorldSpaceBounds();
			float fraction = RayAABox(origin, inv_direction, bounds.mMin - extent, bounds.mMax + extent);
			if (fraction < early_out_fraction)
			{
				// Store hit
				BroadPhaseCastResult result { b, fraction };
				ioCollector.AddHit(result);
				if (ioCollector.ShouldEarlyOut())
					break;
				early_out_fraction = ioCollector.GetEarlyOutFraction();
			}
		}
	}
}

void BroadPhaseBruteForce::CastAABox(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter) const
{
	CastAABoxNoLock(inBox, ioCollector, inBroadPhaseLayerFilter, inObjectLayerFilter);
}

void BroadPhaseBruteForce::FindCollidingPairs(BodyID *ioActiveBodies, int inNumActiveBodies, float inSpeculativeContactDistance, ObjectVsBroadPhaseLayerFilter inObjectVsBroadPhaseLayerFilter, ObjectLayerPairFilter inObjectLayerPairFilter, BodyPairCollector &ioPairCollector) const
{
	shared_lock lock(mMutex);

	// Loop through all active bodies
	size_t num_bodies = mBodyIDs.size();
	for (int b1 = 0; b1 < inNumActiveBodies; ++b1)
	{
		BodyID b1_id = ioActiveBodies[b1];
		const Body &body1 = mBodyManager->GetBody(b1_id);
		const ObjectLayer layer1 = body1.GetObjectLayer();

		// Expand the bounding box by the speculative contact distance
		AABox bounds1 = body1.GetWorldSpaceBounds();
		bounds1.ExpandBy(Vec3::sReplicate(inSpeculativeContactDistance));

		// For all other bodies
		for (size_t b2 = 0; b2 < num_bodies; ++b2)
		{
			// Check if bodies can collide
			BodyID b2_id = mBodyIDs[b2];
			const Body &body2 = mBodyManager->GetBody(b2_id);
			if (!Body::sFindCollidingPairsCanCollide(body1, body2))
				continue;

			// Check if layers can collide
			const ObjectLayer layer2 = body2.GetObjectLayer();
			if (!inObjectLayerPairFilter(layer1, layer2))
				continue;

			// Check if bounds overlap
			const AABox &bounds2 = body2.GetWorldSpaceBounds();
			if (!bounds1.Overlaps(bounds2))
				continue;

			// Store overlapping pair
			ioPairCollector.AddHit({ b1_id, b2_id });
		}
	}
}

JPH_NAMESPACE_END
