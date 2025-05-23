// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/Constraints/PointConstraint.h>
#include <Physics/Body/Body.h>
#include <ObjectStream/TypeDeclarations.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(PointConstraintSettings)
{
	JPH_ADD_BASE_CLASS(PointConstraintSettings, TwoBodyConstraintSettings)

	JPH_ADD_ENUM_ATTRIBUTE(PointConstraintSettings, mSpace)
	JPH_ADD_ATTRIBUTE(PointConstraintSettings, mPoint1)
	JPH_ADD_ATTRIBUTE(PointConstraintSettings, mPoint2)
}

void PointConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{ 
	ConstraintSettings::SaveBinaryState(inStream);

	inStream.Write(mSpace);
	inStream.Write(mPoint1);	
	inStream.Write(mPoint2);
}

void PointConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	inStream.Read(mSpace);
	inStream.Read(mPoint1);
	inStream.Read(mPoint2);
}

TwoBodyConstraint *PointConstraintSettings::Create(Body &inBody1, Body &inBody2) const
{
	return new PointConstraint(inBody1, inBody2, *this);
}

PointConstraint::PointConstraint(Body &inBody1, Body &inBody2, const PointConstraintSettings &inSettings) :
	TwoBodyConstraint(inBody1, inBody2, inSettings),
	mLocalSpacePosition1(inSettings.mPoint1),
	mLocalSpacePosition2(inSettings.mPoint2)
{
	if (inSettings.mSpace == EConstraintSpace::WorldSpace)
	{
		// If all properties were specified in world space, take them to local space now
		mLocalSpacePosition1 = inBody1.GetInverseCenterOfMassTransform() * mLocalSpacePosition1;
		mLocalSpacePosition2 = inBody2.GetInverseCenterOfMassTransform() * mLocalSpacePosition2;
	}
    else if (inSettings.mSpace == EConstraintSpace::LocalToBody)
	{
        const Vec3 world1 = inBody1.GetWorldTransform() * mLocalSpacePosition1;
        const Vec3 world2 = inBody2.GetWorldTransform() * mLocalSpacePosition2;
  
        mLocalSpacePosition1 = inBody1.GetInverseCenterOfMassTransform() * world1;
		mLocalSpacePosition2 = inBody2.GetInverseCenterOfMassTransform() * world2;
	}
}

void PointConstraint::CalculateConstraintProperties()
{	
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, Mat44::sRotation(mBody1->GetRotation()), mLocalSpacePosition1, *mBody2, Mat44::sRotation(mBody2->GetRotation()), mLocalSpacePosition2);
}

void PointConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	CalculateConstraintProperties();
}

void PointConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	// Warm starting: Apply previous frame impulse
	mPointConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
}

bool PointConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	return mPointConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);
}

bool PointConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	// Update constraint properties (bodies may have moved)
	CalculateConstraintProperties();

	return mPointConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, inBaumgarte);
}

#ifdef JPH_DEBUG_RENDERER
void PointConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	// Draw constraint
	inRenderer->DrawMarker(mBody1->GetCenterOfMassTransform() * mLocalSpacePosition1, Color::sRed, 0.1f);
	inRenderer->DrawMarker(mBody2->GetCenterOfMassTransform() * mLocalSpacePosition2, Color::sGreen, 0.1f);
}
#endif // JPH_DEBUG_RENDERER

void PointConstraint::SaveState(StateRecorder &inStream) const
{
	TwoBodyConstraint::SaveState(inStream);

	mPointConstraintPart.SaveState(inStream);
}

void PointConstraint::RestoreState(StateRecorder &inStream)
{
	TwoBodyConstraint::RestoreState(inStream);

	mPointConstraintPart.RestoreState(inStream);
}

Ref<ConstraintSettings> PointConstraint::GetConstraintSettings() const
{
	PointConstraintSettings *settings = new PointConstraintSettings;
	ToConstraintSettings(*settings);
	settings->mSpace = EConstraintSpace::LocalToBodyCOM;
	settings->mPoint1 = mLocalSpacePosition1;
	settings->mPoint2 = mLocalSpacePosition2;
	return settings;
}

JPH_NAMESPACE_END
