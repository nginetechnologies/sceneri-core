// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/Constraints/TwoBodyConstraint.h>
#include <Physics/IslandBuilder.h>
#include <Physics/Body/BodyManager.h>

#ifdef JPH_DEBUG_RENDERER
	#include <Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT(TwoBodyConstraintSettings)
{
	JPH_ADD_BASE_CLASS(TwoBodyConstraintSettings, ConstraintSettings)
}

void TwoBodyConstraint::BuildIslands(uint32 inConstraintIndex, IslandBuilder &ioBuilder, BodyManager &inBodyManager) 
{ 
	// Activate bodies
	BodyID body_ids[2];
	int num_bodies = 0;
	if (mBody1->IsDynamic() && !mBody1->IsActive())
		body_ids[num_bodies++] = mBody1->GetID();
	if (mBody2->IsDynamic() && !mBody2->IsActive())
		body_ids[num_bodies++] = mBody2->GetID();
	if (num_bodies > 0)
		inBodyManager.ActivateBodies(body_ids, num_bodies);

	// Link the bodies into the same island
	ioBuilder.LinkConstraint(inConstraintIndex, mBody1->GetIndexInActiveBodiesInternal(), mBody2->GetIndexInActiveBodiesInternal()); 
}

#ifdef JPH_DEBUG_RENDERER

void TwoBodyConstraint::DrawConstraintReferenceFrame(DebugRenderer *inRenderer) const
{
	Mat44 transform1 = mBody1->GetCenterOfMassTransform() * GetConstraintToBody1Matrix();
	Mat44 transform2 = mBody2->GetCenterOfMassTransform() * GetConstraintToBody2Matrix();
	inRenderer->DrawCoordinateSystem(transform1, 1.1f * mDrawConstraintSize);
	inRenderer->DrawCoordinateSystem(transform2, mDrawConstraintSize);
}

#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_END
