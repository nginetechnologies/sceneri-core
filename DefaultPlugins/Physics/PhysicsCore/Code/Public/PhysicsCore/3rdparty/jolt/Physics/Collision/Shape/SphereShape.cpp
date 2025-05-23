// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/Collision/Shape/SphereShape.h>
#include <Physics/Collision/Shape/ScaleHelpers.h>
#include <Physics/Collision/Shape/GetTrianglesContext.h>
#include <Physics/Collision/RayCast.h>
#include <Physics/Collision/CastResult.h>
#include <Physics/Collision/CollidePointResult.h>
#include <Physics/Collision/TransformedShape.h>
#include <Geometry/RaySphere.h>
#include <Geometry/Plane.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>
#include <ObjectStream/TypeDeclarations.h>
#ifdef JPH_DEBUG_RENDERER
#include <Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(SphereShapeSettings){JPH_ADD_BASE_CLASS(SphereShapeSettings, ConvexShapeSettings)

                                                          JPH_ADD_ATTRIBUTE(SphereShapeSettings, mRadius)}

ShapeSettings::ShapeResult SphereShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new SphereShape(*this, mCachedResult);
	return mCachedResult;
}

SphereShape::SphereShape(const SphereShapeSettings& inSettings, ShapeResult& outResult)
	: ConvexShape(EShapeSubType::Sphere, inSettings, outResult)
	, mRadius(inSettings.mRadius)
{
	if (inSettings.mRadius <= 0.0f)
	{
		outResult.SetError("Invalid radius");
		return;
	}

	outResult.Set(this);
}

float SphereShape::GetScaledRadius(Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Vec3 abs_scale = inScale.Abs();
	return abs_scale.GetX() * mRadius;
}

AABox SphereShape::GetLocalBounds() const
{
	Vec3 half_extent = Vec3::sReplicate(mRadius);
	return AABox(-half_extent, half_extent);
}

AABox SphereShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	float scaled_radius = GetScaledRadius(inScale);
	Vec3 half_extent = Vec3::sReplicate(scaled_radius);
	AABox bounds(-half_extent, half_extent);
	bounds.Translate(inCenterOfMassTransform.GetTranslation());
	return bounds;
}

class SphereShape::SphereNoConvex final : public Support
{
public:
	explicit SphereNoConvex(float inRadius)
		: mRadius(inRadius)
	{
		static_assert(sizeof(SphereNoConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(SphereNoConvex)));
	}

	virtual Vec3 GetSupport(Vec3Arg inDirection) const override
	{
		return Vec3::sZero();
	}

	virtual float GetConvexRadius() const override
	{
		return mRadius;
	}
private:
	float mRadius;
};

class SphereShape::SphereWithConvex final : public Support
{
public:
	explicit SphereWithConvex(float inRadius)
		: mRadius(inRadius)
	{
		static_assert(sizeof(SphereWithConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(SphereWithConvex)));
	}

	virtual Vec3 GetSupport(Vec3Arg inDirection) const override
	{
		float len = inDirection.Length();
		return len > 0.0f ? (mRadius / len) * inDirection : Vec3::sZero();
	}

	virtual float GetConvexRadius() const override
	{
		return 0.0f;
	}
private:
	float mRadius;
};

const ConvexShape::Support* SphereShape::GetSupportFunction(ESupportMode inMode, SupportBuffer& inBuffer, Vec3Arg inScale) const
{
	float scaled_radius = GetScaledRadius(inScale);

	switch (inMode)
	{
		case ESupportMode::IncludeConvexRadius:
			return new (&inBuffer) SphereWithConvex(scaled_radius);

		case ESupportMode::ExcludeConvexRadius:
			return new (&inBuffer) SphereNoConvex(scaled_radius);
	}

	JPH_ASSERT(false);
	return nullptr;
}

MassProperties SphereShape::GetMassProperties(const float density) const
{
	MassProperties p;

	// Calculate mass
	float r2 = mRadius * mRadius;
	p.mMass = (4.0f / 3.0f * JPH_PI) * mRadius * r2 * density;

	// Calculate inertia
	float inertia = (2.0f / 5.0f) * p.mMass * r2;
	p.mInertia = Mat44::sScale(inertia);

	return p;
}

MassProperties SphereShape::GetMassProperties() const
{
    return GetMassProperties(GetMaterial()->GetDensity());
}

Vec3 SphereShape::GetSurfaceNormal(const SubShapeID& inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	float len = inLocalSurfacePosition.Length();
	return len != 0.0f ? inLocalSurfacePosition / len : Vec3::sAxisY();
}

void SphereShape::GetSubmergedVolume(
	Mat44Arg inCenterOfMassTransform,
	Vec3Arg inScale,
	const Plane& inSurface,
	float& outTotalVolume,
	float& outSubmergedVolume,
	Vec3& outCenterOfBuoyancy
) const
{
	float scaled_radius = GetScaledRadius(inScale);
	outTotalVolume = (4.0f / 3.0f * JPH_PI) * Cubed(scaled_radius);

	float distance_to_surface = inSurface.SignedDistance(inCenterOfMassTransform.GetTranslation());
	if (distance_to_surface >= scaled_radius)
	{
		// Above surface
		outSubmergedVolume = 0.0f;
		outCenterOfBuoyancy = Vec3::sZero();
	}
	else if (distance_to_surface <= -scaled_radius)
	{
		// Under surface
		outSubmergedVolume = outTotalVolume;
		outCenterOfBuoyancy = inCenterOfMassTransform.GetTranslation();
	}
	else
	{
		// Intersecting surface

		// Calculate submerged volume, see: https://en.wikipedia.org/wiki/Spherical_cap
		float h = scaled_radius - distance_to_surface;
		outSubmergedVolume = (JPH_PI / 3.0f) * Square(h) * (3.0f * scaled_radius - h);

		// Calculate center of buoyancy, see: http://mathworld.wolfram.com/SphericalCap.html (eq 10)
		float z = (3.0f / 4.0f) * Square(2.0f * scaled_radius - h) / (3.0f * scaled_radius - h);
		outCenterOfBuoyancy = inCenterOfMassTransform.GetTranslation() -
		                      z * inSurface.GetNormal(); // Negative normal since we want the portion under the water

#ifdef JPH_DEBUG_RENDERER
		// Draw intersection between sphere and water plane
		if (sDrawSubmergedVolumes)
		{
			Vec3 circle_center = inCenterOfMassTransform.GetTranslation() - distance_to_surface * inSurface.GetNormal();
			float circle_radius = sqrt(Square(scaled_radius) - Square(distance_to_surface));
			DebugRenderer::sInstance->DrawPie(
				circle_center,
				circle_radius,
				inSurface.GetNormal(),
				inSurface.GetNormal().GetNormalizedPerpendicular(),
				-JPH_PI,
				JPH_PI,
				Color::sGreen,
				DebugRenderer::ECastShadow::Off
			);
		}
#endif // JPH_DEBUG_RENDERER
	}

#ifdef JPH_DEBUG_RENDERER
	// Draw center of buoyancy
	if (sDrawSubmergedVolumes)
		DebugRenderer::sInstance->DrawWireSphere(outCenterOfBuoyancy, 0.05f, Color::sRed, 1);
#endif // JPH_DEBUG_RENDERER
}

#ifdef JPH_DEBUG_RENDERER
void SphereShape::Draw(
	DebugRenderer* inRenderer,
	Mat44Arg inCenterOfMassTransform,
	Vec3Arg inScale,
	ColorArg inColor,
	bool inUseMaterialColors,
	bool inDrawWireframe
) const
{
	DebugRenderer::EDrawMode draw_mode = inDrawWireframe ? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;
	inRenderer->DrawUnitSphere(
		inCenterOfMassTransform * Mat44::sScale(mRadius * inScale.Abs().GetX()),
		inUseMaterialColors ? GetMaterial()->GetDebugColor() : inColor,
		DebugRenderer::ECastShadow::On,
		draw_mode
	);
}
#endif // JPH_DEBUG_RENDERER

bool SphereShape::CastRay(const RayCast& inRay, const SubShapeIDCreator& inSubShapeIDCreator, RayCastResult& ioHit) const
{
	float fraction = RaySphere(inRay.mOrigin, inRay.mDirection, Vec3::sZero(), mRadius);
	if (fraction < ioHit.mFraction)
	{
		ioHit.mFraction = fraction;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}
	return false;
}

void SphereShape::CastRay(
	const RayCast& inRay,
	const RayCastSettings& inRayCastSettings,
	const SubShapeIDCreator& inSubShapeIDCreator,
	CastRayCollector& ioCollector,
	const ShapeFilter& inShapeFilter
) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(inSubShapeIDCreator.GetID()))
		return;

	float min_fraction, max_fraction;
	int num_results = RaySphere(inRay.mOrigin, inRay.mDirection, Vec3::sZero(), mRadius, min_fraction, max_fraction);
	if (num_results > 0 // Ray should intersect
		&& max_fraction >= 0.0f // End of ray should be inside sphere
		&& min_fraction < ioCollector.GetEarlyOutFraction()) // Start of ray should be before early out fraction
	{
		// Better hit than the current hit
		RayCastResult hit;
		hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
		hit.mSubShapeID2 = inSubShapeIDCreator.GetID();

		// Check front side hit
		if (inRayCastSettings.mTreatConvexAsSolid || min_fraction > 0.0f)
		{
			hit.mFraction = max(0.0f, min_fraction);
			ioCollector.AddHit(hit);
		}

		// Check back side hit
		if (inRayCastSettings.mBackFaceMode == EBackFaceMode::CollideWithBackFaces 
			&& num_results > 1 // Ray should have 2 intersections
			&& max_fraction < ioCollector.GetEarlyOutFraction()) // End of ray should be before early out fraction
		{
			hit.mFraction = max_fraction;
			ioCollector.AddHit(hit);
		}
	}
}

void SphereShape::CollidePoint(
	Vec3Arg inPoint, const SubShapeIDCreator& inSubShapeIDCreator, CollidePointCollector& ioCollector, const ShapeFilter& inShapeFilter
) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(inSubShapeIDCreator.GetID()))
		return;

	if (inPoint.LengthSq() <= Square(mRadius))
		ioCollector.AddHit({TransformedShape::sGetBodyID(ioCollector.GetContext()), inSubShapeIDCreator.GetID()});
}

void SphereShape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector& ioCollector) const
{
	Vec3 scale;
	Mat44 transform = inCenterOfMassTransform.Decompose(scale);
	TransformedShape ts(transform.GetTranslation(), transform.GetRotation().GetQuaternion(), this, BodyID(), SubShapeIDCreator());
	ts.SetShapeScale(ScaleHelpers::MakeUniformScale(scale.Abs()));
	ioCollector.AddHit(ts);
}

void SphereShape::GetTrianglesStart(
	GetTrianglesContext& ioContext, const AABox& inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale
) const
{
	float scaled_radius = GetScaledRadius(inScale);
	new (&ioContext) GetTrianglesContextVertexList(
		inPositionCOM,
		inRotation,
		Vec3::sReplicate(1.0f),
		Mat44::sScale(scaled_radius),
		sUnitSphereTriangles.data(),
		sUnitSphereTriangles.size(),
		GetMaterial()
	);
}

int SphereShape::GetTrianglesNext(
	GetTrianglesContext& ioContext, int inMaxTrianglesRequested, Float3* outTriangleVertices, const PhysicsMaterial** outMaterials
) const
{
	return ((GetTrianglesContextVertexList&)ioContext).GetTrianglesNext(inMaxTrianglesRequested, outTriangleVertices, outMaterials);
}

void SphereShape::SaveBinaryState(StreamOut& inStream) const
{
	ConvexShape::SaveBinaryState(inStream);

	inStream.Write(mRadius);
}

void SphereShape::RestoreBinaryState(StreamIn& inStream)
{
	ConvexShape::RestoreBinaryState(inStream);

	inStream.Read(mRadius);
}

bool SphereShape::IsValidScale(Vec3Arg inScale) const
{
	return ConvexShape::IsValidScale(inScale) && ScaleHelpers::IsUniformScale(inScale.Abs());
}

void SphereShape::sRegister()
{
	ShapeFunctions& f = ShapeFunctions::sGet(EShapeSubType::Sphere);
	f.mConstruct = []() -> Shape*
	{
		return new SphereShape;
	};
	f.mColor = Color::sGreen;
}

JPH_NAMESPACE_END
