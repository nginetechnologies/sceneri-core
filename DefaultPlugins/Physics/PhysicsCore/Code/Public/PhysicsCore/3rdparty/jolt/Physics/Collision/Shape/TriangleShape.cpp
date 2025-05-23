// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/Collision/Shape/TriangleShape.h>
#include <Physics/Collision/Shape/ScaleHelpers.h>
#include <Physics/Collision/Shape/GetTrianglesContext.h>
#include <Physics/Collision/RayCast.h>
#include <Physics/Collision/ShapeCast.h>
#include <Physics/Collision/CastResult.h>
#include <Physics/Collision/CollidePointResult.h>
#include <Physics/Collision/TransformedShape.h>
#include <Physics/Collision/CastConvexVsTriangles.h>
#include <Physics/Collision/CastSphereVsTriangles.h>
#include <Physics/Collision/CollideConvexVsTriangles.h>
#include <Physics/Collision/CollideSphereVsTriangles.h>
#include <Physics/Collision/CollisionDispatch.h>
#include <Geometry/ConvexSupport.h>
#include <Geometry/RayTriangle.h>
#include <ObjectStream/TypeDeclarations.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(TriangleShapeSettings)
{
	JPH_ADD_BASE_CLASS(TriangleShapeSettings, ConvexShapeSettings)

	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mV1)
	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mV2)
	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mV3)
	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mConvexRadius)
}

ShapeSettings::ShapeResult TriangleShapeSettings::Create() const
{ 
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new TriangleShape(*this, mCachedResult); 
	return mCachedResult;
}

TriangleShape::TriangleShape(const TriangleShapeSettings &inSettings, ShapeResult &outResult) :
	ConvexShape(EShapeSubType::Triangle, inSettings, outResult), 
	mV1(inSettings.mV1), 
	mV2(inSettings.mV2), 
	mV3(inSettings.mV3), 
	mConvexRadius(inSettings.mConvexRadius) 
{ 
	if (inSettings.mConvexRadius < 0.0f)
	{
		outResult.SetError("Invalid convex radius");
		return;
	}

	outResult.Set(this);
}

AABox TriangleShape::GetLocalBounds() const 
{ 
	AABox bounds(mV1, mV1);
	bounds.Encapsulate(mV2);
	bounds.Encapsulate(mV3);
	bounds.ExpandBy(Vec3::sReplicate(mConvexRadius));
	return bounds;
}
		
AABox TriangleShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Vec3 v1 = inCenterOfMassTransform * (inScale * mV1);
	Vec3 v2 = inCenterOfMassTransform * (inScale * mV2);
	Vec3 v3 = inCenterOfMassTransform * (inScale * mV3);

	AABox bounds(v1, v1);
	bounds.Encapsulate(v2);
	bounds.Encapsulate(v3);
	bounds.ExpandBy(inScale * mConvexRadius);
	return bounds;
}

class TriangleShape::TriangleNoConvex final : public Support
{
public:
							TriangleNoConvex(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3) :
		mTriangleSuport(inV1, inV2, inV3)
	{ 
		static_assert(sizeof(TriangleNoConvex) <= sizeof(SupportBuffer), "Buffer size too small"); 
		JPH_ASSERT(IsAligned(this, alignof(TriangleNoConvex)));
	}

	virtual Vec3			GetSupport(Vec3Arg inDirection) const override
	{ 
		return mTriangleSuport.GetSupport(inDirection);
	}

	virtual float			GetConvexRadius() const override
	{
		return 0.0f;
	}

private:
	TriangleConvexSupport	mTriangleSuport;
};

class TriangleShape::TriangleWithConvex final : public Support
{
public:
							TriangleWithConvex(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, float inConvexRadius) :
		mConvexRadius(inConvexRadius),
		mTriangleSuport(inV1, inV2, inV3)
	{ 
		static_assert(sizeof(TriangleWithConvex) <= sizeof(SupportBuffer), "Buffer size too small"); 
		JPH_ASSERT(IsAligned(this, alignof(TriangleWithConvex)));
	}

	virtual Vec3			GetSupport(Vec3Arg inDirection) const override
	{ 
		Vec3 support = mTriangleSuport.GetSupport(inDirection);
		float len = inDirection.Length();
		if (len > 0.0f)
			support += (mConvexRadius / len) * inDirection;
		return support;
	}

	virtual float			GetConvexRadius() const override
	{
		return mConvexRadius;
	}

private:
	float					mConvexRadius;
	TriangleConvexSupport	mTriangleSuport;
};

const ConvexShape::Support *TriangleShape::GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const
{
	switch (inMode)
	{
	case ESupportMode::IncludeConvexRadius:
		if (mConvexRadius > 0.0f)
			return new (&inBuffer) TriangleWithConvex(inScale * mV1, inScale * mV2, inScale * mV3, mConvexRadius);
		[[fallthrough]];

	case ESupportMode::ExcludeConvexRadius:
		return new (&inBuffer) TriangleNoConvex(inScale * mV1, inScale * mV2, inScale * mV3);
	}

	JPH_ASSERT(false);
	return nullptr;
}

void TriangleShape::GetSupportingFace(Vec3Arg inDirection, Vec3Arg inScale, SupportingFace &outVertices) const
{
	outVertices.push_back(inScale * mV1);
	outVertices.push_back(inScale * mV2);
	outVertices.push_back(inScale * mV3);
}

MassProperties TriangleShape::GetMassProperties([[maybe_unused]] const float density) const
{
	// Object should always be static, return default mass properties
	return MassProperties();
}

MassProperties TriangleShape::GetMassProperties() const
{
	// Object should always be static, return default mass properties
	return MassProperties();
}

Vec3 TriangleShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const 
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID"); 

	Vec3 cross = (mV2 - mV1).Cross(mV3 - mV1);
	float len = cross.Length(); 
	return len != 0.0f? cross / len : Vec3::sAxisY();
}

void TriangleShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy) const
{
	// A triangle has no volume
	outTotalVolume = outSubmergedVolume = 0.0f;
	outCenterOfBuoyancy = Vec3::sZero();
}

#ifdef JPH_DEBUG_RENDERER
void TriangleShape::Draw(DebugRenderer *inRenderer, Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	Vec3 v1 = inCenterOfMassTransform * (inScale * mV1);
	Vec3 v2 = inCenterOfMassTransform * (inScale * mV2);
	Vec3 v3 = inCenterOfMassTransform * (inScale * mV3);

	if (ScaleHelpers::IsInsideOut(inScale))
		swap(v1, v2);

	if (inDrawWireframe)
		inRenderer->DrawWireTriangle(v1, v2, v3, inUseMaterialColors? GetMaterial()->GetDebugColor() : inColor);
	else
		inRenderer->DrawTriangle(v1, v2, v3, inUseMaterialColors? GetMaterial()->GetDebugColor() : inColor);
}
#endif // JPH_DEBUG_RENDERER

bool TriangleShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	float fraction = RayTriangle(inRay.mOrigin, inRay.mDirection, mV1, mV2, mV3);
	if (fraction < ioHit.mFraction)
	{
		ioHit.mFraction = fraction;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}
	return false;
}

void TriangleShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(inSubShapeIDCreator.GetID()))
		return;

	// Back facing check
	if (inRayCastSettings.mBackFaceMode == EBackFaceMode::IgnoreBackFaces && (mV2 - mV1).Cross(mV3 - mV1).Dot(inRay.mDirection) > 0.0f)
		return;

	// Test ray against triangle
	float fraction = RayTriangle(inRay.mOrigin, inRay.mDirection, mV1, mV2, mV3);
	if (fraction < ioCollector.GetEarlyOutFraction())
	{
		// Better hit than the current hit
		RayCastResult hit;
		hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
		hit.mFraction = fraction;
		hit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		ioCollector.AddHit(hit);
	}
}

void TriangleShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Can't be inside a triangle
}

void TriangleShape::sCollideConvexVsTriangle(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetType() == EShapeType::Convex);
	const ConvexShape *shape1 = static_cast<const ConvexShape *>(inShape1);
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape2 = static_cast<const TriangleShape *>(inShape2);

	CollideConvexVsTriangles collider(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	collider.Collide(shape2->mV1, shape2->mV2, shape2->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

void TriangleShape::sCollideSphereVsTriangle(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::Sphere);
	const SphereShape *shape1 = static_cast<const SphereShape *>(inShape1);
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape2 = static_cast<const TriangleShape *>(inShape2);

	CollideSphereVsTriangles collider(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	collider.Collide(shape2->mV1, shape2->mV2, shape2->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

void TriangleShape::sCastConvexVsTriangle(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape = static_cast<const TriangleShape *>(inShape);

	CastConvexVsTriangles caster(inShapeCast, inShapeCastSettings, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	caster.Cast(shape->mV1, shape->mV2, shape->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

void TriangleShape::sCastSphereVsTriangle(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape = static_cast<const TriangleShape *>(inShape);

	CastSphereVsTriangles caster(inShapeCast, inShapeCastSettings, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	caster.Cast(shape->mV1, shape->mV2, shape->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

void TriangleShape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const
{
	Vec3 scale;
	Mat44 transform = inCenterOfMassTransform.Decompose(scale);
	TransformedShape ts(transform.GetTranslation(), transform.GetRotation().GetQuaternion(), this, BodyID(), SubShapeIDCreator());
	ts.SetShapeScale(mConvexRadius == 0.0f? scale : scale.GetSign() * ScaleHelpers::MakeUniformScale(scale.Abs()));
	ioCollector.AddHit(ts);
}

class TriangleShape::TSGetTrianglesContext
{
public:
					TSGetTrianglesContext(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3) : mV1(inV1), mV2(inV2), mV3(inV3) { }

	Vec3			mV1;
	Vec3			mV2;
	Vec3			mV3;

	bool			mIsDone = false;
};

void TriangleShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	static_assert(sizeof(TSGetTrianglesContext) <= sizeof(GetTrianglesContext), "GetTrianglesContext too small");
	JPH_ASSERT(IsAligned(&ioContext, alignof(TSGetTrianglesContext)));

	Mat44 m = Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale);

	new (&ioContext) TSGetTrianglesContext(m * mV1, m * mV2, m * mV3);
}

int TriangleShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	static_assert(cGetTrianglesMinTrianglesRequested >= 3, "cGetTrianglesMinTrianglesRequested is too small");
	JPH_ASSERT(inMaxTrianglesRequested >= cGetTrianglesMinTrianglesRequested);

	TSGetTrianglesContext &context = (TSGetTrianglesContext &)ioContext;

	// Only return the triangle the 1st time
	if (context.mIsDone)
		return 0;
	context.mIsDone = true;

	// Store triangle
	context.mV1.StoreFloat3(outTriangleVertices);
	context.mV2.StoreFloat3(outTriangleVertices + 1);
	context.mV3.StoreFloat3(outTriangleVertices + 2);

	// Store material
	if (outMaterials != nullptr)
		*outMaterials = GetMaterial();

	return 1;
}

void TriangleShape::SaveBinaryState(StreamOut &inStream) const
{
	ConvexShape::SaveBinaryState(inStream);

	inStream.Write(mV1);
	inStream.Write(mV2);
	inStream.Write(mV3);
	inStream.Write(mConvexRadius);
}

void TriangleShape::RestoreBinaryState(StreamIn &inStream)
{
	ConvexShape::RestoreBinaryState(inStream);

	inStream.Read(mV1);
	inStream.Read(mV2);
	inStream.Read(mV3);
	inStream.Read(mConvexRadius);
}

bool TriangleShape::IsValidScale(Vec3Arg inScale) const
{
	return ConvexShape::IsValidScale(inScale) && (mConvexRadius == 0.0f || ScaleHelpers::IsUniformScale(inScale.Abs()));
}

void TriangleShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::Triangle);
	f.mConstruct = []() -> Shape * { return new TriangleShape; };
	f.mColor = Color::sGreen;

	for (EShapeSubType s : sConvexSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::Triangle, sCollideConvexVsTriangle);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::Triangle, sCastConvexVsTriangle);
	}

	// Specialized collision functions
	CollisionDispatch::sRegisterCollideShape(EShapeSubType::Sphere, EShapeSubType::Triangle, sCollideSphereVsTriangle);
	CollisionDispatch::sRegisterCastShape(EShapeSubType::Sphere, EShapeSubType::Triangle, sCastSphereVsTriangle);
}

JPH_NAMESPACE_END
